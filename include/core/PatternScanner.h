#pragma once
#include <cstdint>
#include "sdk/Signatures.h"

namespace Core {

    class PatternScanner {
    public:
        // Scans the loaded main module (kenshi_x64.exe) for an IDA-style AOB pattern
        static uintptr_t FindPattern(const char* pattern);

        // Resolves a signature entry using AOB scanning first, with optional RVA fallback
        static uintptr_t ResolveSignature(const KenshiSDK::Signatures::SignatureEntry& sig, bool allowFallbackRVA = false);

        // Returns base address of kenshi_x64.exe
        static uintptr_t GetModuleBase();
    };

} // namespace Core


