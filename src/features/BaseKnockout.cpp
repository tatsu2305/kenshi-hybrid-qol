#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/Logger.h"
#include "sdk/GameApi.h"
#include "sdk/Signatures.h"
#include <Windows.h>
#include <unordered_map>

// Feature 4 (stage 1): hostile characters knocked out inside a player-owned town are not
// allowed to "get up" (MedicalSystem::canGetUpWakeUp) so they cannot re-aggro Attack Town.
// Guards / Corpse-Furnace workers can then deal with them while they are down.
namespace Features::BaseKnockout {

    namespace V = KenshiSDK::Verified;

    using CanGetUpFn = bool(__fastcall*)(void* medicalSystem);
    using SafeGetUpFn = bool(__fastcall*)(void* character);
    static CanGetUpFn  s_origCanGetUp  = nullptr;
    static SafeGetUpFn s_origSafeGetUp = nullptr;

    static std::unordered_map<const void*, ULONGLONG> s_heldSince;   // character -> first tick we held it
    static ULONGLONG s_lastLog = 0;

    // POD-only so it can use SEH. Returns true when `character` is a hostile (relation <= -30
    // towards the town owner) non-player character inside a player-owned town.
    static bool EvalHostileInPlayerTown(void* character) {
        __try {
            if (!character) return false;
            if (V::ReadAt<bool>(character, V::Character::IsChained))     return false; // prisoners
            if (V::ReadAt<bool>(character, V::Character::IsBeingCarried)) return false;

            void* faction = GameApi::GetFaction(character);
            if (!faction || GameApi::IsPlayerFaction(faction)) return false;

            void* town = GameApi::GetCurrentTown(character);
            if (!town) return false;
            void* owner = V::ReadAt<void*>(town, V::RootObjectBase::Owner_FactionPtr);
            if (!owner || !GameApi::IsPlayerFaction(owner)) return false;

            return GameApi::IsEnemyFaction(faction, owner);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ShouldHoldDown(void* character) {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableBaseKnockoutFix) return false;
        if (!EvalHostileInPlayerTown(character)) {
            if (!s_heldSince.empty()) s_heldSince.erase(character);
            return false;
        }
        const ULONGLONG now = GetTickCount64();
        auto it = s_heldSince.find(character);
        if (it == s_heldSince.end()) {
            if (s_heldSince.size() > 4096) s_heldSince.clear();
            it = s_heldSince.emplace(character, now).first;
            Core::Logger::Debug("[BaseKnockout] holding hostile %p down inside player town.", character);
        }
        if (cfg.holdDownMaxSeconds > 0 && (now - it->second) > static_cast<ULONGLONG>(cfg.holdDownMaxSeconds) * 1000ULL) {
            s_heldSince.erase(it);   // timeout reached: let vanilla logic run
            return false;
        }
        return true;
    }

    static bool __fastcall DetourCanGetUp(void* medical) {
        const bool vanilla = s_origCanGetUp ? s_origCanGetUp(medical) : true;
        if (!vanilla || !medical) return vanilla;
        void* character = reinterpret_cast<uint8_t*>(medical) - V::MedicalSystem::OwnerFromThis;
        return ShouldHoldDown(character) ? false : vanilla;
    }

    static bool __fastcall DetourIsItSafeToGetUp(void* character) {
        const bool vanilla = s_origSafeGetUp ? s_origSafeGetUp(character) : true;
        if (!vanilla) return vanilla;
        return ShouldHoldDown(character) ? false : vanilla;
    }

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableBaseKnockoutFix) { Core::Logger::Info("[BaseKnockout] disabled via config."); return true; }
        if (!GameApi::Init()) return false;
        using namespace KenshiSDK::Signatures;
        bool ok = Core::HookManager::InstallHook(SIG_MEDICAL_CAN_GET_UP_WAKE_UP,
            reinterpret_cast<void*>(&DetourCanGetUp), &s_origCanGetUp);
        if (cfg.hookIsItSafeToGetUp) {
            ok = Core::HookManager::InstallHook(SIG_CHARACTER_IS_IT_SAFE_TO_GET_UP,
                reinterpret_cast<void*>(&DetourIsItSafeToGetUp), &s_origSafeGetUp) && ok;
        }
        return ok;
    }
}
