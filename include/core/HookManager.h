#pragma once
#include <cstdint>
#include "sdk/Signatures.h"
#include "sdk/SignaturesPending.h"

namespace Core {

    class HookManager {
    public:
        static bool Initialize();
        static void Shutdown();

        template <typename FuncPtr>
        static bool InstallHook(
            const KenshiSDK::Signatures::SignatureEntry& sig,
            void* detourFunc,
            FuncPtr* outOriginalFunc,
            bool allowFallbackRVA = false)  // ignored: guessed RVAs were removed
        {
            uintptr_t targetAddr = ResolveAndCreateHook(
                sig,
                detourFunc,
                reinterpret_cast<void**>(outOriginalFunc),
                allowFallbackRVA
            );
            return targetAddr != 0;
        }

    private:
        static uintptr_t ResolveAndCreateHook(
            const KenshiSDK::Signatures::SignatureEntry& sig,
            void* detourFunc,
            void** outOriginalFunc,
            bool allowFallbackRVA = false);
    };

} // namespace Core



