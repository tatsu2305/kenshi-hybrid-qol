#include "core/PatternScanner.h"
#include "core/Logger.h"
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <cstdlib>

namespace Core {

    uintptr_t PatternScanner::GetModuleBase() {
        static uintptr_t s_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        return s_base;
    }

    static void ParsePattern(const char* pattern, std::vector<int>& outBytes) {
        outBytes.clear();
        const char* cur = pattern;
        while (*cur) {
            if (*cur == ' ') {
                ++cur;
                continue;
            }
            if (*cur == '?') {
                ++cur;
                if (*cur == '?') ++cur;
                outBytes.push_back(-1);
            } else {
                char* endPtr = nullptr;
                int byteVal = static_cast<int>(std::strtoul(cur, &endPtr, 16));
                outBytes.push_back(byteVal);
                cur = endPtr;
            }
        }
    }

    uintptr_t PatternScanner::FindPattern(const char* pattern) {
        HMODULE hModule = GetModuleHandleA(nullptr);
        if (!hModule) return 0;

        MODULEINFO modInfo{};
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
            return 0;
        }

        std::vector<int> patternBytes;
        ParsePattern(pattern, patternBytes);
        if (patternBytes.empty()) return 0;

        const uint8_t* scanBytes = reinterpret_cast<const uint8_t*>(modInfo.lpBaseOfDll);
        const size_t imageSize = modInfo.SizeOfImage;
        const size_t patSize = patternBytes.size();
        const int* patData = patternBytes.data();

        if (imageSize < patSize) return 0;

        for (size_t i = 0; i <= imageSize - patSize; ++i) {
            bool found = true;
            for (size_t j = 0; j < patSize; ++j) {
                if (patData[j] != -1 && scanBytes[i + j] != static_cast<uint8_t>(patData[j])) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return reinterpret_cast<uintptr_t>(&scanBytes[i]);
            }
        }
        return 0;
    }

    uintptr_t PatternScanner::ResolveSignature(const KenshiSDK::Signatures::SignatureEntry& sig, bool /*allowFallbackRVA (removed: no guessed RVAs)*/) {
        if (!sig.pattern) {
            Logger::Warn("[Scanner] '%s' has no AOB pattern yet - hook skipped.", sig.name);
            return 0;
        }
        uintptr_t addr = FindPattern(sig.pattern);
        if (addr == 0) {
            Logger::Error("[Scanner] AOB for '%s' not found in this build - hook skipped.", sig.name);
            return 0;
        }
        Logger::Info("[Scanner] Resolved '%s' at 0x%llX", sig.name, static_cast<unsigned long long>(addr));
        return addr;
    }
} // namespace Core



