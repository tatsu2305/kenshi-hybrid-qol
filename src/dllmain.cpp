#include <Windows.h>
#include <string>
#include "core/Config.h"
#include "core/Logger.h"
#include "core/HookManager.h"
#include "features/Features.h"

namespace {

    std::string GetModuleDirectory(HMODULE hModule) {
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(hModule, path, MAX_PATH);
        std::string fullPath(path);
        size_t pos = fullPath.find_last_of("\\/");
        return (pos != std::string::npos) ? fullPath.substr(0, pos) : ".";
    }

    unsigned LogPeTimestamp() {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        return nt->FileHeader.TimeDateStamp;
    }

    void InitializePlugin(HMODULE hModule) {
        std::string baseDir = GetModuleDirectory(hModule);
        std::string iniPath = baseDir + "\\KenshiHybridQoL.ini";

        // Fall back to config.ini if KenshiHybridQoL.ini is not found
        if (GetFileAttributesA(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            iniPath = baseDir + "\\config.ini";
        }

        Core::Config::Load(iniPath);
        const auto& cfg = Core::Config::Get();

        Core::Logger::Init(baseDir + "\\KenshiHybridQoL.log", cfg.enableDebugLog);
        Core::Logger::Info("==============================================================");
        Core::Logger::Info("Kenshi Hybrid QoL & AI Fix Plugin (v1.0.65 x64) Initializing");
        Core::Logger::Info("Loaded configuration from: %s", iniPath.c_str());
        Core::Logger::Info("SmeltBelowRarity threshold: %s", Core::Config::RarityTierToString(cfg.smeltBelowRarity));
        Core::Logger::Info("==============================================================");

        if (!Core::HookManager::Initialize()) {
            Core::Logger::Error("Failed to initialize HookManager. Aborting plugin load.");
            return;
        }

        Core::Logger::Info("Host exe PE timestamp: 0x%08X (0x65D604D7 = RE_Kenshi Steam 1.0.65 copy, 0x6602D59D = newer top-level exe; AOB patterns match both)", LogPeTimestamp());
        Features::BulkHauling::Init();
        Features::AntiGateClipping::Init();
        Features::TacticalEscort::Init();
        Features::BaseKnockout::Init();
        Features::SmartLootSmelt::Init();
        Features::TurretDiscipline::Init();

        Core::Logger::Info("All enabled Kenshi Hybrid QoL hooks registered.");
    }

    void ShutdownPlugin() {
        Core::HookManager::Shutdown();
        Core::Logger::Info("Kenshi Hybrid QoL & AI Fix Plugin unloaded cleanly.");
        Core::Logger::Shutdown();
    }

} // anonymous namespace

// Optional RE_Kenshi / Ogre plugin export entry point
extern "C" __declspec(dllexport) void dllStartPlugin() {
    // Invoked automatically if loaded via RE_Kenshi's Plugins_x64.cfg
}

extern "C" __declspec(dllexport) void dllStopPlugin() {
    ShutdownPlugin();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializePlugin(hModule);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        ShutdownPlugin();
    }
    return TRUE;
}



