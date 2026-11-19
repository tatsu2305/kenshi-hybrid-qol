#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/Logger.h"
#include "sdk/Signatures.h"
#include <Windows.h>
#include <unordered_map>

// Feature 6 (stage 1): TurretBuilding::operate(Character*, float) is the per-tick "use turret"
// call. We register every character seen inside it (IsActiveGunner). That alone does not stop
// a gunner from leaving - the decision to drop the turret lives in AI code that is not named
// in KenshiLib. As an OPTIONAL experiment (config: BlockMeleeGoalsForGunners) we hook
// Character::addGoal and drop melee/chase goals for active gunners.
namespace Features::TurretDiscipline {

    namespace V = KenshiSDK::Verified;

    using OperateFn = void(__fastcall*)(void* turret, void* who, float dt);
    using AddGoalFn = void(__fastcall*)(void* character, int taskType, void* subject);
    static OperateFn s_origOperate = nullptr;
    static AddGoalFn s_origAddGoal = nullptr;

    static std::unordered_map<const void*, ULONGLONG> s_gunners;  // character -> last operate tick
    static unsigned s_blocked = 0;

    bool IsActiveGunner(const void* character) {
        auto it = s_gunners.find(character);
        if (it == s_gunners.end()) return false;
        return (GetTickCount64() - it->second) <= static_cast<ULONGLONG>(Core::Config::Get().gunnerGraceMs);
    }

    static void __fastcall DetourOperate(void* turret, void* who, float dt) {
        if (who) {
            if (s_gunners.size() > 2048) s_gunners.clear();
            s_gunners[who] = GetTickCount64();
        }
        if (s_origOperate) s_origOperate(turret, who, dt);
    }

    static bool IsMeleeGoal(int t) {
        return t == V::TaskType::MELEE_ATTACK || t == V::TaskType::FOCUSED_MELEE_ATTACK ||
               t == V::TaskType::CHOOSE_ENEMY_AND_ATTACK || t == V::TaskType::ATTACK_ENEMIES ||
               t == V::TaskType::UNPROVOKED_FOCUSED_MELEE || t == V::TaskType::CHASE;
    }

    // Diagnostic (TraceHaulingCalls = true): record subjects of the tasks needed to design the
    // unimplemented modules - BODYGUARD(45) subject should be the protected character,
    // FEED_CORPSE_INTO_MACHINE(136) / OPERATE_MACHINERY(87) the furnace/body.
    static void TraceTask(void* character, int taskType, void* subject) {
        if (taskType != 44 && taskType != 45 && taskType != 87 && taskType != 136 && taskType != 26 && taskType != 177 && taskType != 181 && taskType != 184 && taskType != 241) return;
        static unsigned seen[300] = {};
        if (taskType >= 300 || ++seen[taskType] > 25) return;
        Core::Logger::Info("[TaskTrace] addGoal type=%d char=%p subject=%p", taskType, character, subject);
    }

    static void __fastcall DetourAddGoal(void* character, int taskType, void* subject) {
        if (Core::Config::Get().traceHaulingCalls) TraceTask(character, taskType, subject);
        if (Core::Config::Get().enableTacticalEscort && TacticalEscort::ShouldDropGoal(character, taskType, subject)) return;
        if (Core::Config::Get().blockMeleeGoalsForGunners && IsMeleeGoal(taskType) && IsActiveGunner(character)) {
            if ((++s_blocked % 50) == 1)
                Core::Logger::Debug("[TurretDiscipline] blocked melee goal %d for gunner %p (total %u)", taskType, character, s_blocked);
            return;
        }
        if (s_origAddGoal) s_origAddGoal(character, taskType, subject);
    }

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableTurretHoldDiscipline) { Core::Logger::Info("[TurretDiscipline] disabled via config."); return true; }
        using namespace KenshiSDK::Signatures;
        bool ok = Core::HookManager::InstallHook(SIG_TURRET_OPERATE,
            reinterpret_cast<void*>(&DetourOperate), &s_origOperate);
        if (cfg.blockMeleeGoalsForGunners || cfg.enableTacticalEscort) {
            Core::Logger::Warn("[TurretDiscipline] addGoal hook ON (gunner blocking=%d, escort=%d).", cfg.blockMeleeGoalsForGunners ? 1 : 0, cfg.enableTacticalEscort ? 1 : 0);
            ok = Core::HookManager::InstallHook(SIG_CHARACTER_ADD_GOAL,
                reinterpret_cast<void*>(&DetourAddGoal), &s_origAddGoal) && ok;
        }
        return ok;
    }
}
