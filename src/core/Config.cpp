#include "core/Config.h"
#include "core/Logger.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>

namespace Core {

    ModConfig Config::s_config;

    static std::string ReadIniString(const char* section, const char* key, const char* defaultVal, const std::string& path) {
        char buffer[256] = { 0 };
        GetPrivateProfileStringA(section, key, defaultVal, buffer, sizeof(buffer), path.c_str());
        return std::string(buffer);
    }

    static bool ReadIniBool(const char* section, const char* key, bool defaultVal, const std::string& path) {
        std::string val = ReadIniString(section, key, defaultVal ? "true" : "false", path);
        std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return (val == "true" || val == "1" || val == "yes" || val == "on");
    }

    static float ReadIniFloat(const char* section, const char* key, float defaultVal, const std::string& path) {
        std::string val = ReadIniString(section, key, "", path);
        if (val.empty()) return defaultVal;
        try {
            return std::stof(val);
        } catch (...) {
            return defaultVal;
        }
    }

    static int32_t ReadIniInt(const char* section, const char* key, int32_t defaultVal, const std::string& path) {
        return static_cast<int32_t>(GetPrivateProfileIntA(section, key, defaultVal, path.c_str()));
    }

    KenshiSDK::RarityTier Config::ParseRarityTier(const std::string& rarityStr) {
        std::string lower = rarityStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "prototype" || lower == "grey" || lower == "0") return KenshiSDK::RarityTier::PROTOTYPE;
        if (lower == "shoddy" || lower == "white" || lower == "1")   return KenshiSDK::RarityTier::SHODDY;
        if (lower == "standard" || lower == "2")                     return KenshiSDK::RarityTier::STANDARD;
        if (lower == "high" || lower == "green" || lower == "3")     return KenshiSDK::RarityTier::HIGH;
        if (lower == "blue" || lower == "specialist" || lower == "4") return KenshiSDK::RarityTier::BLUE;
        if (lower == "yellow" || lower == "masterwork" || lower == "edge" || lower == "5") return KenshiSDK::RarityTier::YELLOW;
        if (lower == "meitou" || lower == "orange" || lower == "6")  return KenshiSDK::RarityTier::MEITOU;

        return KenshiSDK::RarityTier::BLUE;
    }

    const char* Config::RarityTierToString(KenshiSDK::RarityTier tier) {
        switch (tier) {
            case KenshiSDK::RarityTier::PROTOTYPE: return "Prototype (Grey)";
            case KenshiSDK::RarityTier::SHODDY:    return "Shoddy (White)";
            case KenshiSDK::RarityTier::STANDARD:  return "Standard (Green-Low)";
            case KenshiSDK::RarityTier::HIGH:      return "High (Green-High)";
            case KenshiSDK::RarityTier::BLUE:      return "Blue (Specialist / Mk I-III)";
            case KenshiSDK::RarityTier::YELLOW:    return "Yellow (Masterwork / Edge)";
            case KenshiSDK::RarityTier::MEITOU:    return "Meitou (Cross)";
            default:                               return "Unknown";
        }
    }

    bool Config::Load(const std::string& filePath) {
        // [General]
        s_config.enableDebugLog          = ReadIniBool("General", "EnableDebugLog", true, filePath);
        s_config.allowFallbackRVAs       = ReadIniBool("General", "AllowFallbackRVAs", true, filePath);

        // [Features]
        s_config.enableSmartBulkHauling  = ReadIniBool("Features", "EnableSmartBulkHauling", true, filePath);
        s_config.enableAntiGateClipping  = ReadIniBool("Features", "EnableAntiGateClipping", true, filePath);
        s_config.enableTacticalEscort    = ReadIniBool("Features", "EnableTacticalEscort", true, filePath);
        s_config.enableBaseKnockoutFix   = ReadIniBool("Features", "EnableBaseKnockoutFix", true, filePath);
        s_config.enableSmartLootAndSmelt = ReadIniBool("Features", "EnableSmartLootAndSmelt", true, filePath);
        s_config.enableTurretHoldDiscipline = ReadIniBool("Features", "EnableTurretHoldDiscipline", true, filePath);

        // [BulkHauling]
        s_config.maxBatchTransferLimit   = ReadIniInt("BulkHauling", "MaxBatchTransferLimit", 50, filePath);
        s_config.enableBulkFetch         = ReadIniBool("BulkHauling", "EnableBulkFetch", false, filePath);
        s_config.fetchBatchLimit         = ReadIniInt("BulkHauling", "FetchBatchLimit", 10, filePath);
        s_config.encumbranceSafetyMargin = ReadIniFloat("BulkHauling", "EncumbranceSafetyMargin", 0.90f, filePath);

        // [AntiGateClipping]
        s_config.gateProximityRadius     = ReadIniFloat("AntiGateClipping", "GateProximityRadius", 45.0f, filePath);
        s_config.separationForceMultiplier = ReadIniFloat("AntiGateClipping", "SeparationForceMultiplier", 0.05f, filePath);
        s_config.maxFrameDisplacementNearGate = ReadIniFloat("AntiGateClipping", "MaxFrameDisplacementNearGate", 0.12f, filePath);

        // [TacticalEscort]
        s_config.maxEscortLeashDistance  = ReadIniFloat("TacticalEscort", "MaxEscortLeashDistance", 18.0f, filePath);
        s_config.syncSpeedThresholdDistance = ReadIniFloat("TacticalEscort", "SyncSpeedThresholdDistance", 10.0f, filePath);
        s_config.syncVIPMovementSpeed    = ReadIniBool("TacticalEscort", "SyncVIPMovementSpeed", true, filePath);

        // [BaseKnockout]
        s_config.hostileWakeupMode       = ReadIniInt("BaseKnockout", "HostileWakeupMode", 2, filePath);
        s_config.allowLiveHostileFurnaceDisposal = ReadIniBool("BaseKnockout", "AllowLiveHostileFurnaceDisposal", true, filePath);
        s_config.allowGuardFinishOff     = ReadIniBool("BaseKnockout", "AllowGuardFinishOff", true, filePath);

        // [SmartLootAndSmelt]
        s_config.stripBeforeCorpseFurnace = ReadIniBool("SmartLootAndSmelt", "StripBeforeCorpseFurnace", true, filePath);
        std::string rarityStr            = ReadIniString("SmartLootAndSmelt", "SmeltBelowRarity", "Blue", filePath);
        s_config.smeltBelowRarity        = ParseRarityTier(rarityStr);
        s_config.preserveConsumablesAndResources = ReadIniBool("SmartLootAndSmelt", "PreserveConsumablesAndResources", true, filePath);
        s_config.buildingSearchRadius    = ReadIniFloat("SmartLootAndSmelt", "BuildingSearchRadius", 350.0f, filePath);

        // [TurretDiscipline]
        s_config.minWallElevationDelta   = ReadIniFloat("TurretDiscipline", "MinWallElevationDelta", 15.0f, filePath);
        s_config.blindspotSuppressionRadius = ReadIniFloat("TurretDiscipline", "BlindspotSuppressionRadius", 35.0f, filePath);
        s_config.hardLockTurretTask      = ReadIniBool("TurretDiscipline", "HardLockTurretTask", true, filePath);
        s_config.holdDownMaxSeconds      = ReadIniInt("BaseKnockout", "HoldDownMaxSeconds", 0, filePath);
        s_config.hookIsItSafeToGetUp     = ReadIniBool("BaseKnockout", "HookIsItSafeToGetUp", true, filePath);
        s_config.traceHaulingCalls       = ReadIniBool("BulkHauling", "TraceHaulingCalls", false, filePath);
        s_config.blockMeleeGoalsForGunners = ReadIniBool("TurretDiscipline", "BlockMeleeGoalsForGunners", false, filePath);
        s_config.gunnerGraceMs           = ReadIniInt("TurretDiscipline", "GunnerGraceMs", 1500, filePath);

        return true;
    }

    const ModConfig& Config::Get() {
        return s_config;
    }

} // namespace Core



