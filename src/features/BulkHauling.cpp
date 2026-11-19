#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/PatternScanner.h"
#include "core/Logger.h"
#include "sdk/Signatures.h"
#include <Windows.h>
#include <intrin.h>
#include <map>
#include <algorithm>

// Feature 1 - Smart Bulk Hauling (Steam 1.0.65, RE_Kenshi exe PE timestamp 0x65D604D7 only).
//
// Reverse-engineering result (scratch/dis_callers.py + disassembly of the delivery task update,
// function starting ~RVA 0x3453D0): for every material the building still needs, the task does
//     item = Inventory::getItem(mat);
//     ConstructionState::addMaterials(state, item->data);     // call returns to 0x348A53, +1.0f
//     Inventory::removeItemAutoDestroy(inv, item, 1);          // call returns to 0x348A67
//     Building::addConstructionProgress(0.0f);
// i.e. ONE unit per material per task tick. We hook addMaterials + removeItemAutoDestroy and, only
// when they are called from exactly those two call sites, deliver up to MaxBatchTransferLimit units
// of that material at once, capped by (a) the stack size in the carrier's inventory and (b) the
// building's remaining need (needsMat), so nothing is over-delivered or removed beyond what exists.
// The 3 AI callers of getNeededMats (hasBuildingMaterials/findBuildingMaterials/
// findResourceDestinationFor) only choose what/where to fetch and are left untouched.
//
// Optional tracer: [BulkHauling] TraceHaulingCalls = true logs callers of needsMat/getNeededMats.
namespace Features::BulkHauling {

    using NeedsMatFn  = int(__fastcall*)(void* constructionState, void* gameData);
    using GetNeededFn = void(__fastcall*)(void* constructionState, void* outList);
    using AddMatsFn   = void(__fastcall*)(void* constructionState, void* gameData);
    using RemoveFn    = bool(__fastcall*)(void* inventory, void* item, int howMany);
    static NeedsMatFn s_origNeedsMat = nullptr;
    static GetNeededFn s_origGetNeeded = nullptr;
    static AddMatsFn s_origAddMats = nullptr;
    static RemoveFn s_origRemove = nullptr;
    using TakeFn = void*(__fastcall*)(void* inventory, void* item, int howMany, bool returnCopy);
    static TakeFn s_origTake = nullptr;
    static std::map<unsigned long long, unsigned> s_takeSeen;
    using TakeOneFn = void*(__fastcall*)(void* inventory, void* gameData);
    static TakeOneFn s_origTakeOne = nullptr;
    // Return addresses after the two takeOneItemOnly calls in the fetch task (RVAs, 0x65D604D7 build).
    constexpr uintptr_t RVA_RET_TAKEONE_A = 0x33CF1C;   // confirmed in-game trace: fetch loop at 0x33CF17
    constexpr uintptr_t RVA_RET_TAKEONE_B = 0x33CF1C;
    static NeedsMatFn s_needsMatDirect = nullptr;   // resolved address, used for remaining-need queries

    // Return addresses inside the delivery loop (RVAs, PE timestamp 0x65D604D7 build).
    constexpr uintptr_t RVA_RET_AFTER_ADD_MATERIALS = 0x348A53;
    constexpr uintptr_t RVA_RET_AFTER_REMOVE_ITEM   = 0x348A67;
    constexpr uint32_t  SUPPORTED_PE_TIMESTAMP      = 0x65D604D7;
    constexpr size_t    ITEM_QUANTITY_OFFSET        = 0x12C;  // Item::quantity (KenshiLib)

    static void* s_pendingState = nullptr;  // set by addMaterials detour, consumed by remove detour
    static void* s_pendingMat = nullptr;
    static unsigned s_bulkEvents = 0;
    static unsigned long long s_bulkExtraUnits = 0;

    static std::map<uintptr_t, unsigned> s_needsMatCallers, s_getNeededCallers;

    static void Note(std::map<uintptr_t, unsigned>& m, const char* what, void* ret) {
        const uintptr_t rva = reinterpret_cast<uintptr_t>(ret) - Core::PatternScanner::GetModuleBase();
        if (m.size() > 256 && m.find(rva) == m.end()) return;
        unsigned& n = m[rva];
        if (++n == 1) Core::Logger::Info("[BulkHauling/trace] %s called from RVA 0x%llX (new caller)", what, static_cast<unsigned long long>(rva));
        else if (n == 1000 || n == 100000) Core::Logger::Info("[BulkHauling/trace] %s RVA 0x%llX hit %u times", what, static_cast<unsigned long long>(rva), n);
    }

    static int __fastcall DetourNeedsMat(void* cs, void* gd) {
        Note(s_needsMatCallers, "needsMat", _ReturnAddress());
        return s_origNeedsMat ? s_origNeedsMat(cs, gd) : 0;
    }
    static void __fastcall DetourGetNeeded(void* cs, void* out) {
        Note(s_getNeededCallers, "getNeededMats", _ReturnAddress());
        if (s_origGetNeeded) s_origGetNeeded(cs, out);
    }

    // Tracer for the FETCH phase: which call sites withdraw items from an inventory and how many.
    static void* __fastcall DetourTake(void* inv, void* item, int howMany, bool copy) {
        const uintptr_t rva = reinterpret_cast<uintptr_t>(_ReturnAddress()) - Core::PatternScanner::GetModuleBase();
        const unsigned long long key = (static_cast<unsigned long long>(rva) << 32) | static_cast<unsigned>(howMany);
        if (s_takeSeen.size() < 300 && ++s_takeSeen[key] == 1)
            Core::Logger::Info("[BulkHauling/trace] removeItemDontDestroy from RVA 0x%llX howMany=%d", static_cast<unsigned long long>(rva), howMany);
        return s_origTake(inv, item, howMany, copy);
    }

    // FETCH phase (opt-in, [BulkHauling] EnableBulkFetch): the fetch task calls takeOneItemOnly, which
    // splits exactly one unit off the storage stack. Right after it, we move extra units from the
    // remaining storage stack into the returned item: S->quantity -= k, X->quantity += k. The total
    // unit count is preserved (no items created/destroyed), at least 1 unit stays in storage.
    static void* __fastcall DetourTakeOne(void* inv, void* mat) {
        void* x = s_origTakeOne(inv, mat);
        const uintptr_t rva = reinterpret_cast<uintptr_t>(_ReturnAddress()) - Core::PatternScanner::GetModuleBase();
        if (Core::Config::Get().traceHaulingCalls) {
            static unsigned t = 0;
            if (++t <= 40) Core::Logger::Info("[BulkHauling/trace] takeOneItemOnly from RVA 0x%llX qty=%d", static_cast<unsigned long long>(rva),
                x ? *reinterpret_cast<int*>(static_cast<char*>(x) + ITEM_QUANTITY_OFFSET) : -1);
        }
        if (!x || !inv || !mat || (rva != RVA_RET_TAKEONE_A && rva != RVA_RET_TAKEONE_B)) return x;
        char* xi = static_cast<char*>(x);
        int& xq = *reinterpret_cast<int*>(xi + ITEM_QUANTITY_OFFSET);
        if (xq != 1) return x;
        // Inventory layout used by takeOneItemOnly itself: count at +0x18, Item* array at +0x20.
        const unsigned count = *reinterpret_cast<unsigned*>(static_cast<char*>(inv) + 0x18);
        void** arr = *reinterpret_cast<void***>(static_cast<char*>(inv) + 0x20);
        if (!arr || count > 100000) return x;
        const int cap = (std::max)(1, Core::Config::Get().fetchBatchLimit);
        for (unsigned i = 0; i < count; ++i) {
            char* it = static_cast<char*>(arr[i]);
            if (!it || it == xi) continue;
            using GetDataFn = void*(__fastcall*)(void*);
            void* data = (*reinterpret_cast<GetDataFn**>(it))[3](it);   // vtable +0x18, as in takeOneItemOnly
            if (data != mat) continue;
            int& sq = *reinterpret_cast<int*>(it + ITEM_QUANTITY_OFFSET);
            const int k = (std::min)(sq - 1, cap - 1);
            if (k > 0) {
                sq -= k; xq += k;
                static unsigned n = 0;
                if (++n <= 5 || (n % 500) == 0)
                    Core::Logger::Info("[BulkHauling] bulk fetch: took %d units at once (storage keeps %d, events %u)", xq, sq, n);
            }
            break;
        }
        return x;
    }

    static void __fastcall DetourAddMaterials(void* state, void* mat) {
        const uintptr_t rva = reinterpret_cast<uintptr_t>(_ReturnAddress()) - Core::PatternScanner::GetModuleBase();
        if (rva == RVA_RET_AFTER_ADD_MATERIALS) { s_pendingState = state; s_pendingMat = mat; }
        else { s_pendingState = nullptr; s_pendingMat = nullptr; }
        s_origAddMats(state, mat);
    }

    static bool __fastcall DetourRemoveItem(void* inv, void* item, int howMany) {
        const uintptr_t rva = reinterpret_cast<uintptr_t>(_ReturnAddress()) - Core::PatternScanner::GetModuleBase();
        if (rva == RVA_RET_AFTER_REMOVE_ITEM && howMany == 1 && s_pendingState && s_pendingMat && item) {
            void* state = s_pendingState; void* mat = s_pendingMat;
            s_pendingState = nullptr; s_pendingMat = nullptr;
            const int stack = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(item) + ITEM_QUANTITY_OFFSET);
            const int cap = (std::max)(1, Core::Config::Get().maxBatchTransferLimit);
            int extra = 0;
            if (stack > 1 && stack < 1000000 && s_needsMatDirect) {
                const int want = (std::min)(stack, cap) - 1;
                while (extra < want && s_needsMatDirect(state, mat) > 0) {
                    s_origAddMats(state, mat);   // +1.0f delivered, same as the vanilla call
                    ++extra;
                }
            }
            if (extra > 0) {
                ++s_bulkEvents; s_bulkExtraUnits += static_cast<unsigned>(extra);
                if (s_bulkEvents <= 5 || (s_bulkEvents % 500) == 0)
                    Core::Logger::Info("[BulkHauling] delivered %d units in one tick (stack %d, still needed %d, events %u, extra units %llu)",
                        1 + extra, stack, s_needsMatDirect(state, mat), s_bulkEvents, s_bulkExtraUnits);
            }
            return s_origRemove(inv, item, 1 + extra);
        }
        return s_origRemove(inv, item, howMany);
    }

    static uint32_t HostPeTimestamp() {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        return nt->FileHeader.TimeDateStamp;
    }

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableSmartBulkHauling) { Core::Logger::Info("[BulkHauling] disabled via config."); return true; }
        using namespace KenshiSDK::Signatures;
        bool ok = true;

        if (HostPeTimestamp() != SUPPORTED_PE_TIMESTAMP) {
            Core::Logger::Warn("[BulkHauling] host exe is not the 0x65D604D7 build; delivery call-site RVAs differ, bulk delivery NOT installed.");
        } else {
            s_needsMatDirect = reinterpret_cast<NeedsMatFn>(Core::PatternScanner::ResolveSignature(SIG_CONSTRUCTION_NEEDS_MAT));
            if (!s_needsMatDirect) {
                Core::Logger::Warn("[BulkHauling] could not resolve needsMat; bulk delivery NOT installed.");
            } else {
                ok = Core::HookManager::InstallHook(SIG_CONSTRUCTION_ADD_MATERIALS,
                        reinterpret_cast<void*>(&DetourAddMaterials), &s_origAddMats);
                ok = Core::HookManager::InstallHook(SIG_INVENTORY_REMOVE_AUTO_DESTROY,
                        reinterpret_cast<void*>(&DetourRemoveItem), &s_origRemove) && ok;
                Core::Logger::Info("[BulkHauling] bulk delivery %s (max %d units per tick per material).",
                    ok ? "ACTIVE" : "FAILED to install", cfg.maxBatchTransferLimit);
            }
        }

        if (cfg.enableBulkFetch && HostPeTimestamp() == SUPPORTED_PE_TIMESTAMP) {
            const bool f = Core::HookManager::InstallHook(SIG_INVENTORY_TAKE_ONE_ONLY,
                reinterpret_cast<void*>(&DetourTakeOne), &s_origTakeOne);
            Core::Logger::Info("[BulkHauling] bulk fetch %s (max %d units per take).", f ? "ACTIVE" : "FAILED", cfg.fetchBatchLimit);
            ok = f && ok;
        }
        if (cfg.traceHaulingCalls) {
            Core::HookManager::InstallHook(SIG_INVENTORY_REMOVE_DONT_DESTROY,
                reinterpret_cast<void*>(&DetourTake), &s_origTake);
            ok = Core::HookManager::InstallHook(SIG_CONSTRUCTION_NEEDS_MAT,
                reinterpret_cast<void*>(&DetourNeedsMat), &s_origNeedsMat) && ok;
            ok = Core::HookManager::InstallHook(SIG_CONSTRUCTION_GET_NEEDED_MATS,
                reinterpret_cast<void*>(&DetourGetNeeded), &s_origGetNeeded) && ok;
        }
        return ok;
    }
}
