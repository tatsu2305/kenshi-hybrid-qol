#pragma once
#include <string>

namespace Core {

    class Logger {
    public:
        static void Init(const std::string& logFilePath, bool enableDebug);
        static void Shutdown();

        static void Info(const char* fmt, ...);
        static void Warn(const char* fmt, ...);
        static void Error(const char* fmt, ...);
        static void Debug(const char* fmt, ...);

    private:
        static void WriteLog(const char* level, const char* fmt, va_list args);
        static bool s_debugEnabled;
    };

} // namespace Core

