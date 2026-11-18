#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace gf
{
    enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };

    class Logger
    {
    public:
#ifdef NDEBUG
        static constexpr LogLevel DefaultLevel = LogLevel::Info;
#else
        static constexpr LogLevel DefaultLevel = LogLevel::Trace;
#endif

        explicit Logger(const std::filesystem::path& file = {}, LogLevel minimum = DefaultLevel);
        void write(LogLevel level, std::string_view message) const noexcept;

    private:
        LogLevel minimum_;
        mutable std::mutex mutex_;
        mutable std::ofstream file_;
    };
}
