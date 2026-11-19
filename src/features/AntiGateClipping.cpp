#include "features/Features.h"
#include "core/Config.h"
#include "core/HookManager.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>

namespace Features::AntiGateClipping {

    using namespace KenshiSDK;

    using ApplySeparationForceFn = void(__fastcall*)(
        Character* self,
        Character* neighbor,
        Vector3* ioPushVector,
        float deltaTime
    );

    static ApplySeparationForceFn s_origApplySeparationForce = nullptr;

    static bool IsNearGateOrWallSegment(const Character* character, float radius) {
        if (!character) return false;
        const float radiusSq = radius * radius;

        // Check AI's direct target building first (e.g. repairing gate or attacking gate)
        if (character->ai && character->ai->targetBuilding) {
            const Building* bld = character->ai->targetBuilding;
            if (bld->functionType == BuildingFunction::BF_GATE || bld->functionType == BuildingFunction::BF_WALL) {
                if (character->position.distanceSquared(bld->position) <= radiusSq) {
                    return true;
                }
            }
        }

        // Check parent town buildings for gate/wall proximity
        const TownBase* town = character->currentTown;
        if (town && town->buildings && town->buildingCount > 0) {
            for (int32_t i = 0; i < town->buildingCount; ++i) {
                const Building* bld = town->buildings[i];
                if (!bld) continue;
                if (bld->functionType == BuildingFunction::BF_GATE || bld->functionType == BuildingFunction::BF_WALL) {
                    if (character->position.distanceSquared(bld->position) <= radiusSq) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void FilterSeparationVector(
        Character* self,
        Character* neighbor,
        Vector3* ioPushVector,
        float /*deltaTime*/)
    {
        if (!self || !ioPushVector) return;

        const auto& cfg = Core::Config::Get();
        if (IsNearGateOrWallSegment(self, cfg.gateProximityRadius) ||
            (neighbor && IsNearGateOrWallSegment(neighbor, cfg.gateProximityRadius)))
        {
            // Scale horizontal separation force down and zero vertical impulse so characters
            // cannot push each other through closed gates or up/out of wall NavMesh boundaries.
            ioPushVector->x *= cfg.separationForceMultiplier;
            ioPushVector->y = 0.0f;
            ioPushVector->z *= cfg.separationForceMultiplier;

            const float maxDisp = cfg.maxFrameDisplacementNearGate;
            const float magSq = ioPushVector->x * ioPushVector->x + ioPushVector->z * ioPushVector->z;
            if (magSq > maxDisp * maxDisp && magSq > 0.00001f) {
                const float invMag = maxDisp / std::sqrt(magSq);
                ioPushVector->x *= invMag;
                ioPushVector->z *= invMag;
            }
        }
    }

    static void __fastcall DetourApplySeparationForce(
        Character* self,
        Character* neighbor,
        Vector3* ioPushVector,
        float deltaTime)
    {
        if (s_origApplySeparationForce) {
            s_origApplySeparationForce(self, neighbor, ioPushVector, deltaTime);
        }

        if (Core::Config::Get().enableAntiGateClipping) {
            FilterSeparationVector(self, neighbor, ioPushVector, deltaTime);
        }
    }

    bool Init() {
        const auto& cfg = Core::Config::Get();
        if (!cfg.enableAntiGateClipping) {
            Core::Logger::Info("[AntiGateClipping] Disabled via config.ini.");
            return true;
        }

        return Core::HookManager::InstallHook(
            Signatures::SIG_APPLY_SEPARATION_FORCE,
            reinterpret_cast<void*>(&DetourApplySeparationForce),
            &s_origApplySeparationForce,
            cfg.allowFallbackRVAs
        );
    }

} // namespace Features::AntiGateClipping

