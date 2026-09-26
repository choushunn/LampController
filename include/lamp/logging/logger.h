#pragma once

#include <functional>
#include <string>

namespace lamp {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// Central logging facility. Thread-safe: a process-wide mutex serializes every
// write so output from multiple devices never interleaves. The file sink is
// disabled by default; when enabled, messages append to
// lampctl-YYYYMMDD.log inside the configured directory and files larger than
// 1 MB are rotated to <name>.old on the next write.
class Logger {
public:
    explicit Logger(LogLevel level = LogLevel::Info);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void SetLevel(LogLevel level);
    LogLevel Level() const;

    void SetFileSink(bool enabled, const std::string& directory);
    void AddSink(std::function<void(const std::string&)> sink);

    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);

    static std::string Format(const char* format, ...);

private:
    struct Impl;
    Impl* impl_;
    void LogImpl(LogLevel level, const std::string& message);
};

}  // namespace lamp
