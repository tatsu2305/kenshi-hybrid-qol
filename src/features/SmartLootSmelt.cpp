#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/PatternScanner.h"
#include "core/Logger.h"
#include "sdk/Signatures.h"
#include <Windows.h>
#include <intrin.h>
#include <map>

// Feature 5 - Smart Loot & Smelt: DIAGNOSTIC STAGE ONLY (no gameplay change).
// The previous implementation used an invented struct model and has been removed. The real
// implementation needs (a) the code path that moves a body's items, (b) the furnace deposit call and
// (c) item rarity data - none are located yet. With [BulkHauling] TraceHaulingCalls = true this logs:
//   * FurnaceBuilding::operate(Character*, float): furnace + operator pointers (first 30 distinct pairs)
//   * Inventory::addItem call sites (return-address RVA, quantity) - first 80 distinct callers, to find
//     the code that strips/moves items around corpse disposal and furnace feeding.
// Combine with the [TaskTrace] lines for FEED_CORPSE_INTO_MACHINE (136) / OPERATE_MACHINERY (87).
namespace Features::SmartLootSmelt {

    using FurnaceOperateFn = void(__fastcall*)(void* furnace, void* who, float amount);
    using AddItemFn = bool(__fastcall*)(void* inv, void* item, int qty, bool dropOnFail, bool destroyOnFail);
    static FurnaceOperateFn s_origOperate = nullptr;
    static AddItemFn s_origAddItem = nullptr;
    static std::map<unsigned long long, unsigned> s_pairs;
    static std::map<unsigned long long, unsigned> s_callers;

    static void __fastcall DetourFurnaceOperate(void* furnace, void* who, float amount) {
        const unsigned long long k = (reinterpret_cast<unsigned long long>(furnace) * 1315423911ull) ^ reinterpret_cast<unsigned long long>(who);
        if (s_pairs.size() < 30 && ++s_pairs[k] == 1)
            Core::Logger::Info("[SmartLootSmelt/trace] FurnaceBuilding::operate furnace=%p who=%p amount=%.2f", furnace, who, amount);
        s_origOperate(furnace, who, amount);
    }

    // Item::quality is a float at +0x11C (KenshiLib Item.h); read defensively.
    static float ReadQuality(const void* item) {
        __try { return *reinterpret_cast<const float*>(static_cast<const char*>(item) + 0x11C); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return -1.0f; }
    }

    static bool __fastcall DetourAddItem(void* inv, void* item, int qty, bool dropOnFail, bool destroyOnFail) {
        const unsigned long long rva = reinterpret_cast<unsigned long long>(_ReturnAddress()) - Core::PatternScanner::GetModuleBase();
        if (s_callers.size() < 200 && ++s_callers[rva] == 1)
            Core::Logger::Info("[SmartLootSmelt/trace] Inventory::addItem from RVA 0x%llX qty=%d drop=%d destroy=%d quality=%.2f inv=%p", rva, qty, dropOnFail ? 1 : 0, destroyOnFail ? 1 : 0, ReadQuality(item), inv);
        return s_origAddItem(inv, item, qty, dropOnFail, destroyOnFail);
    }

    void ProcessCorpseBeforeFurnace(KenshiSDK::Character*, KenshiSDK::Character*, KenshiSDK::Building*) {}  // legacy API, unused

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableSmartLootAndSmelt) { Core::Logger::Info("[SmartLootSmelt] disabled via config."); return true; }
        if (!cfg.traceHaulingCalls) {
            Core::Logger::Info("[SmartLootSmelt] not implemented yet (diagnostic only; set TraceHaulingCalls = true to log furnace/item traces).");
            return true;
        }
        using namespace KenshiSDK::Signatures;
        bool ok = Core::HookManager::InstallHook(SIG_FURNACE_OPERATE, reinterpret_cast<void*>(&DetourFurnaceOperate), &s_origOperate);
        ok = Core::HookManager::InstallHook(SIG_INVENTORY_ADD_ITEM, reinterpret_cast<void*>(&DetourAddItem), &s_origAddItem) && ok;
        Core::Logger::Info("[SmartLootSmelt] diagnostic tracing %s.", ok ? "ACTIVE" : "FAILED");
        return ok;
    }
}
