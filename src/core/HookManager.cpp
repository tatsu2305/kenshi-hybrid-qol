#include "core/HookManager.h"
#include "core/PatternScanner.h"
#include "core/Logger.h"
#include <MinHook.h>

namespace Core {

    bool HookManager::Initialize() {
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            Logger::Error("[HookManager] MH_Initialize failed with status: %d", static_cast<int>(status));
            return false;
        }
        Logger::Info("[HookManager] MinHook initialized successfully.");
        return true;
    }

    void HookManager::Shutdown() {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Logger::Info("[HookManager] All hooks disabled and MinHook uninitialized.");
    }

    uintptr_t HookManager::ResolveAndCreateHook(
        const KenshiSDK::Signatures::SignatureEntry& sig,
        void* detourFunc,
        void** outOriginalFunc,
        bool allowFallbackRVA)
    {
        uintptr_t targetAddr = PatternScanner::ResolveSignature(sig, allowFallbackRVA);
        if (targetAddr == 0) {
            return 0;
        }

        MH_STATUS status = MH_CreateHook(
            reinterpret_cast<LPVOID>(targetAddr),
            detourFunc,
            outOriginalFunc
        );

        if (status != MH_OK) {
            Logger::Error("[HookManager] MH_CreateHook failed for '%s' at 0x%llX (status=%d)",
                sig.name, static_cast<unsigned long long>(targetAddr), static_cast<int>(status));
            return 0;
        }

        status = MH_EnableHook(reinterpret_cast<LPVOID>(targetAddr));
        if (status != MH_OK) {
            Logger::Error("[HookManager] MH_EnableHook failed for '%s' at 0x%llX (status=%d)",
                sig.name, static_cast<unsigned long long>(targetAddr), static_cast<int>(status));
            return 0;
        }

        Logger::Info("[HookManager] Hook enabled for '%s' at 0x%llX",
            sig.name, static_cast<unsigned long long>(targetAddr));
        return targetAddr;
    }

} // namespace Core

