#include "Logger.hpp"
#include <chrono>
#include <format>
#include <iostream>

namespace gf
{
    namespace
    {
        std::string_view label(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Trace:    return "TRACE";
            case LogLevel::Debug:    return "DEBUG";
            case LogLevel::Info:     return "INFO";
            case LogLevel::Warning:  return "WARNING";
            case LogLevel::Error:    return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            }
            return "UNKNOWN";
        }
    }

    Logger::Logger(const std::filesystem::path& file, LogLevel minimum) : minimum_(minimum)
    {
        if (file.empty()) return;
        file_.open(file, std::ios::app);
        if (!file_)
            write(LogLevel::Warning, "Could not open the log file. Logging to the console.");
    }

    void Logger::write(LogLevel level, std::string_view message) const noexcept
    {
        if (level < minimum_) return;
        try
        {
            const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            const auto line = std::format("{:%F %T}Z [{}] {}\n", now, label(level), message);
            const std::lock_guard lock(mutex_);
            auto& console = level >= LogLevel::Error ? std::cerr : std::clog;
            console << line << std::flush;
            if (file_.is_open()) file_ << line << std::flush;
        }
        catch (...)
        {
            // Diagnostics must be safe during error handling and in window callbacks.
        }
    }
}
