#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/Logger.h"
#include "core/PatternScanner.h"
#include "sdk/Signatures.h"
#include <Windows.h>
#include <unordered_map>
#include <cmath>

// Feature 3 - Tactical Escort (stage 1, experimental).
// Verified pieces (KenshiLib headers + Steam 1.0.65 signatures):
//   * Player orders reach Character::addJob(TaskType, RootObject* subject, ...) and
//     Character::addOrder(Building*, TaskType, RootObject* subject, ...). For TaskType::BODYGUARD (45)
//     `subject` is the protected character, so we record guard -> VIP there.
//   * Character position = RootObjectBase::pos at +0x48 (3 floats).
//   * The goal-drop happens in Character::addGoal (hooked in TurretDiscipline.cpp, which calls
//     ShouldDropGoal): when a guard that is farther than MaxEscortLeashDistance from its VIP is given
//     a melee/attack goal, the goal is dropped so the BODYGUARD job pulls it back to the VIP.
// NOT implemented: VIP speed sync (no known API to set movement speed) and AI-side bodyguard update.
// Diagnostics: with TraceHaulingCalls = true, addJob/addOrder calls for BODYGUARD/FOLLOW/FEED_CORPSE/
// OPERATE_MACHINERY are logged as [TaskTrace] (subject pointers).
namespace Features::TacticalEscort {

    constexpr int TT_BODYGUARD = 45;
    constexpr size_t POS_OFFSET = 0x48;

    using AddJobFn   = void(__fastcall*)(void* self, int t, void* subject, bool shift, bool addDontClear, const void* loc);
    using AddOrderFn = void(__fastcall*)(void* self, void* dest, int t, void* subject, bool shift, bool clear, const void* loc);
    static AddJobFn s_origAddJob = nullptr;
    static AddOrderFn s_origAddOrder = nullptr;
    static std::unordered_map<const void*, void*> s_vipOf;   // guard -> protected character
    static std::unordered_map<const void*, void*> s_vipVtbl; // vip -> vtable pointer seen at assignment (stale-pointer check)
    static unsigned s_dropped = 0;

    static bool ReadPos(const void* obj, float out[3]) {
        __try {
            const float* p = reinterpret_cast<const float*>(static_cast<const char*>(obj) + POS_OFFSET);
            out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
            return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static void Track(void* self, int t, void* subject, bool shift) {
        if (t == TT_BODYGUARD && subject && subject != self) {
            if (s_vipOf.size() > 4096) s_vipOf.clear();
            s_vipOf[self] = subject;
            __try { s_vipVtbl[subject] = *reinterpret_cast<void**>(subject); } __except (EXCEPTION_EXECUTE_HANDLER) { s_vipOf.erase(self); return; }
        } else if (!shift && t != TT_BODYGUARD) {
            s_vipOf.erase(self);   // a new non-bodyguard order replaces the escort assignment
        }
        if (Core::Config::Get().traceHaulingCalls && (t == 44 || t == TT_BODYGUARD || t == 87 || t == 136 || t == 26 || t == 177 || t == 181 || t == 184 || t == 241)) {
            static unsigned seen[300] = {};
            if (t < 300 && ++seen[t] <= 25)
                Core::Logger::Info("[TaskTrace] addJob/addOrder type=%d char=%p subject=%p shift=%d", t, self, subject, shift ? 1 : 0);
        }
    }

    // Combat cancel (hooked on per-character update functions): the AI's combat goals live in the
    // character's OrdersReceiver (KenshiLib AITaskSystem.h: `me` at +0x8, hasGoal/removeGoal). When a
    // guard is farther than the leash from its VIP, its attack goals are removed through the game's own
    // removeGoal(); the BODYGUARD job then brings it back. The OrdersReceiver pointer is learned from
    // OrdersReceiver::addGoal calls (AI goals do NOT go through Character::addGoal).
    using TickFn = void(__fastcall*)(void* self);
    using OrdersHasGoalFn = bool(__fastcall*)(void* ors, int taskType);
    using OrdersRemoveGoalFn = void(__fastcall*)(void* ors, int taskType);
    static TickFn s_origUniqueState = nullptr;
    static TickFn s_origFoodUpdate = nullptr;
    static OrdersHasGoalFn s_hasGoal = nullptr;
    static OrdersRemoveGoalFn s_removeGoal = nullptr;
    static unsigned s_tickHits[2] = {0, 0};
    static unsigned s_cancels = 0;

    // Character -> OrdersReceiver, as Character::getOrdersReciever() does (verified by disassembly of the
    // Steam 1.0.65 function): ors = *(*(char**)(self + 0x650) + 0x20).
    static void* GetOrders(void* self) {
        __try {
            char* ai = *reinterpret_cast<char**>(static_cast<char*>(self) + 0x650);
            return ai ? *reinterpret_cast<void**>(ai + 0x20) : nullptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    // Layouts verified by disassembling OrdersReceiver::hasGoal and TaskMatch::key (Steam 1.0.65):
    //   goals[5] at ors+0xA0, stride 0x18: {vtbl, uint count @+8, uint cap, Tasker** data @+0x10};
    //   Tasker* -> TaskData* at +0x70 -> TaskType key (int) at +0x44;  currentGoal (TaskMatch) at ors+0x1C0,
    //   its TaskData* at +0 -> key at +0x44.
    static void DescribeGoals(const void* ors, char* buf, size_t cap) {
        size_t n = 0; buf[0] = 0;
        __try {
            const char* o = static_cast<const char*>(ors);
            const char* cur = *reinterpret_cast<const char* const*>(o + 0x1C0);
            n += snprintf(buf + n, cap - n, "cur=%d lists:", cur ? *reinterpret_cast<const int*>(cur + 0x44) : -1);
            for (int l = 0; l < 5 && n + 16 < cap; ++l) {
                const unsigned cnt = *reinterpret_cast<const unsigned*>(o + 0xA8 + 0x18 * l);
                void* const* data = *reinterpret_cast<void* const* const*>(o + 0xB0 + 0x18 * l);
                n += snprintf(buf + n, cap - n, " [%d]", l);
                for (unsigned i = 0; i < cnt && i < 12 && data && n + 12 < cap; ++i) {
                    const char* t = static_cast<const char*>(data[i]);
                    const char* td = t ? *reinterpret_cast<const char* const*>(t + 0x70) : nullptr;
                    n += snprintf(buf + n, cap - n, " %d", td ? *reinterpret_cast<const int*>(td + 0x44) : -1);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { snprintf(buf, cap, "unreadable"); }
    }

    static void CheckAliveOwned(void* vip, void* vipVtbl, void* ors, void* self, bool& alive, bool& owned) {
        __try {
            alive = (*reinterpret_cast<void**>(vip) == vipVtbl);
            owned = (*reinterpret_cast<void**>(static_cast<char*>(ors) + 0x8) == self);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static void Tick(void* self, int which) {
        if (s_vipOf.empty()) return;
        if (Core::Config::Get().traceHaulingCalls && ++s_tickHits[which] == 1)
            Core::Logger::Info("[TacticalEscort/trace] update hook %d is firing", which);
        auto it = s_vipOf.find(self);
        if (it == s_vipOf.end()) return;
        void* ors = GetOrders(self);
        if (Core::Config::Get().traceHaulingCalls) {   // status line every 5 s per guard
            static std::unordered_map<const void*, ULONGLONG> lastStatus;
            const ULONGLONG now = GetTickCount64();
            ULONGLONG& ls = lastStatus[self];
            if (now - ls >= 2000) {
                ls = now;
                float pa[3] = {0}, pb[3] = {0};
                const bool okp = ReadPos(self, pa) && ReadPos(it->second, pb);
                const float ddx = pa[0] - pb[0], ddy = pa[1] - pb[1], ddz = pa[2] - pb[2];
                char goals[64] = "n/a";
                if (ors && s_hasGoal) {
                    static const int g[] = {16, 4, 5, 9, 61, 15, 46};
                    int n = 0;
                    for (int t : g) n += snprintf(goals + n, sizeof(goals) - n, "%d:%d ", t, s_hasGoal(ors, t) ? 1 : 0);
                }
                char all[256] = "";
                if (ors) DescribeGoals(ors, all, sizeof(all));
                Core::Logger::Info("[TacticalEscort/status] guard=%p vip=%p dist=%.1f posOk=%d ors=%p goals(type:has)=%s | %s",
                    self, it->second, okp ? std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz) : -1.0f, okp ? 1 : 0,
                    ors, goals, all);
            }
        }
        if (!ors || !s_hasGoal || !s_removeGoal) return;
        float a[3], b[3];
        if (!ReadPos(self, a) || !ReadPos(it->second, b)) return;
        bool alive = false, owned = false;
        CheckAliveOwned(it->second, s_vipVtbl[it->second], ors, self, alive, owned);
        if (!alive) { s_vipOf.erase(it); return; }
        if (!owned) return;
        const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        const float leash = Core::Config::Get().maxEscortLeashDistance;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= leash * leash || d2 > 500.0f * 500.0f) return;   // >500m: VIP is in another area / not really nearby
        // ATTACK_ENEMIES, MELEE_ATTACK, FOCUSED_MELEE_ATTACK, CHOOSE_ENEMY_AND_ATTACK, UNPROVOKED_FOCUSED_MELEE_ATTACK, PROTECT_ALLIES
        // (PROTECT_ALLIES=15 was the only combat-like goal seen on a bodyguard in the in-game goal dump; 31=STAY_CLOSE_TO_TARGET is the
        // bodyguard's own follow goal and is never removed, 32=SELF_PRESERVATION is left alone.)
        static const int attackGoals[] = {16, 4, 5, 9, 61, 15};
        for (int t : attackGoals) {
            if (s_hasGoal(ors, t)) {
                s_removeGoal(ors, t);
                if ((++s_cancels % 10) == 1)
                    Core::Logger::Info("[TacticalEscort] guard %p is %.1fm from VIP (leash %.1fm): removed attack goal %d (cancels %u)",
                        self, std::sqrt(d2), leash, t, s_cancels);
            }
        }
    }
    static void __fastcall DetourUniqueState(void* self) { Tick(self, 0); s_origUniqueState(self); }
    static void __fastcall DetourFoodUpdate(void* self)  { Tick(self, 1); s_origFoodUpdate(self); }

    static void __fastcall DetourAddJob(void* self, int t, void* subject, bool shift, bool add, const void* loc) {
        Track(self, t, subject, shift);
        s_origAddJob(self, t, subject, shift, add, loc);
    }
    static void __fastcall DetourAddOrder(void* self, void* dest, int t, void* subject, bool shift, bool clear, const void* loc) {
        Track(self, t, subject, shift);
        s_origAddOrder(self, dest, t, subject, shift, clear, loc);
    }

    static bool IsAttackGoal(int t) { return t == 4 || t == 5 || t == 9 || t == 16 || t == 61; }

    bool ShouldDropGoal(const void* guard, int taskType, const void*) {
        if (s_vipOf.empty() || !IsAttackGoal(taskType)) return false;
        auto it = s_vipOf.find(guard);
        if (it == s_vipOf.end()) return false;
        float a[3], b[3];
        if (!ReadPos(guard, a) || !ReadPos(it->second, b)) return false;
        const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        const float leash = Core::Config::Get().maxEscortLeashDistance;
        if (dx * dx + dy * dy + dz * dz <= leash * leash) return false;
        if ((++s_dropped % 25) == 1)
            Core::Logger::Info("[TacticalEscort] guard %p dropped attack goal %d: %.1fm from VIP (leash %.1fm), total %u",
                guard, taskType, std::sqrt(dx * dx + dy * dy + dz * dz), leash, s_dropped);
        return true;
    }

    void SynchronizeEscort(KenshiSDK::AIState*, float) {}   // legacy placeholder API, unused

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableTacticalEscort) { Core::Logger::Info("[TacticalEscort] disabled via config."); return true; }
        using namespace KenshiSDK::Signatures;
        bool ok = Core::HookManager::InstallHook(SIG_CHARACTER_ADD_JOB, reinterpret_cast<void*>(&DetourAddJob), &s_origAddJob);
        ok = Core::HookManager::InstallHook(SIG_CHARACTER_ADD_ORDER, reinterpret_cast<void*>(&DetourAddOrder), &s_origAddOrder) && ok;
        s_hasGoal = reinterpret_cast<OrdersHasGoalFn>(Core::PatternScanner::ResolveSignature(SIG_ORDERS_HAS_GOAL));
        s_removeGoal = reinterpret_cast<OrdersRemoveGoalFn>(Core::PatternScanner::ResolveSignature(SIG_ORDERS_REMOVE_GOAL));
        if (!s_hasGoal || !s_removeGoal) { Core::Logger::Warn("[TacticalEscort] hasGoal/removeGoal not resolved; combat cancel disabled."); ok = false; }
        ok = Core::HookManager::InstallHook(SIG_CHARACTER_UNIQUE_STATE_UPDATE, reinterpret_cast<void*>(&DetourUniqueState), &s_origUniqueState) && ok;
        ok = Core::HookManager::InstallHook(SIG_CHARACTER_FOOD_UPDATE, reinterpret_cast<void*>(&DetourFoodUpdate), &s_origFoodUpdate) && ok;
        Core::Logger::Info("[TacticalEscort] %s (leash %.1fm; speed sync not implemented).", ok ? "ACTIVE" : "hook install FAILED", cfg.maxEscortLeashDistance);
        return ok;
    }
}
