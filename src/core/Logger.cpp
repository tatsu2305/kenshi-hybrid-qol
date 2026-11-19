#include "core/Logger.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace Core {

    bool Logger::s_debugEnabled = true;
    static FILE* s_logFile = nullptr;
    static std::mutex s_logMutex;

    void Logger::Init(const std::string& logFilePath, bool enableDebug) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        s_debugEnabled = enableDebug;
        if (!s_logFile) {
            s_logFile = std::fopen(logFilePath.c_str(), "w");
        }
    }

    void Logger::Shutdown() {
        std::lock_guard<std::mutex> lock(s_logMutex);
        if (s_logFile) {
            std::fclose(s_logFile);
            s_logFile = nullptr;
        }
    }

    void Logger::WriteLog(const char* level, const char* fmt, va_list args) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        if (!s_logFile) return;

        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);

        std::fprintf(s_logFile, "[%02d:%02d:%02d] [%s] ",
            localTime.tm_hour, localTime.tm_min, localTime.tm_sec, level);
        std::vfprintf(s_logFile, fmt, args);
        std::fprintf(s_logFile, "\n");
        std::fflush(s_logFile);
    }

    void Logger::Info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        WriteLog("INFO", fmt, args);
        va_end(args);
    }

    void Logger::Warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        WriteLog("WARN", fmt, args);
        va_end(args);
    }

    void Logger::Error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        WriteLog("ERROR", fmt, args);
        va_end(args);
    }

    void Logger::Debug(const char* fmt, ...) {
        if (!s_debugEnabled) return;
        va_list args;
        va_start(args, fmt);
        WriteLog("DEBUG", fmt, args);
        va_end(args);
    }

} // namespace Core

