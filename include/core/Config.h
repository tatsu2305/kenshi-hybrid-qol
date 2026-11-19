#pragma once
#include <string>
#include "sdk/KenshiSDK.h"

namespace Core {

    struct ModConfig {
        // [General]
        bool enableDebugLog = true;
        bool allowFallbackRVAs = true;

        // [Features]
        bool enableSmartBulkHauling = true;
        bool enableAntiGateClipping = true;
        bool enableTacticalEscort = true;
        bool enableBaseKnockoutFix = true;
        bool enableSmartLootAndSmelt = true;
        bool enableTurretHoldDiscipline = true;

        // [BulkHauling]
        int32_t maxBatchTransferLimit = 50;
        bool    enableBulkFetch = false;      // experimental: take several units per fetch from storage
        int32_t fetchBatchLimit = 10;
        float encumbranceSafetyMargin = 0.90f;

        // [AntiGateClipping]
        float gateProximityRadius = 45.0f;
        float separationForceMultiplier = 0.05f;
        float maxFrameDisplacementNearGate = 0.12f;

        // [TacticalEscort]
        float maxEscortLeashDistance = 18.0f;
        float syncSpeedThresholdDistance = 10.0f;
        bool syncVIPMovementSpeed = true;

        // [BaseKnockout]
        int32_t hostileWakeupMode = 2; // 0 = PlayDead, 1 = Flee, 2 = Execute/Furnace
        bool allowLiveHostileFurnaceDisposal = true;
        bool allowGuardFinishOff = true;

        // [SmartLootAndSmelt]
        bool stripBeforeCorpseFurnace = true;
        KenshiSDK::RarityTier smeltBelowRarity = KenshiSDK::RarityTier::BLUE;
        bool preserveConsumablesAndResources = true;
        float buildingSearchRadius = 350.0f;

        // [TurretDiscipline]
        float minWallElevationDelta = 15.0f;
        float blindspotSuppressionRadius = 35.0f;
        bool hardLockTurretTask = true;

        // stage-1 additions
        int32_t holdDownMaxSeconds = 0;          // [BaseKnockout] 0 = hold indefinitely
        bool hookIsItSafeToGetUp = true;         // [BaseKnockout]
        bool traceHaulingCalls = false;          // [BulkHauling]
        bool blockMeleeGoalsForGunners = false;  // [TurretDiscipline] EXPERIMENTAL
        int32_t gunnerGraceMs = 1500;            // [TurretDiscipline]
    };

    class Config {
    public:
        static bool Load(const std::string& filePath);
        static const ModConfig& Get();
        static KenshiSDK::RarityTier ParseRarityTier(const std::string& rarityStr);
        static const char* RarityTierToString(KenshiSDK::RarityTier tier);

    private:
        static ModConfig s_config;
    };

} // namespace Core


