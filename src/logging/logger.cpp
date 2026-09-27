#include "lamp/logging/logger.h"

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

namespace lamp {

namespace {

std::mutex g_write_mutex;

const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

std::string CurrentTimestamp() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u",
                  time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                  time.wSecond);
    return buffer;
}

std::string DateFileKey() {
    SYSTEMTIME time;
    GetLocalTime(&time);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04u%02u%02u", time.wYear,
                  time.wMonth, time.wDay);
    return buffer;
}

void AppendToFile(const std::string& directory, const std::string& line) {
    std::string path = directory;
    if (!path.empty() && path.back() != '\\' && path.back() != '/') {
        path += '\\';
    }
    path += "lampctl-" + DateFileKey() + ".log";

    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    LARGE_INTEGER size;
    if (GetFileSizeEx(handle, &size) && size.QuadPart > (1024 * 1024)) {
        CloseHandle(handle);
        std::string rotated = path + ".old";
        DeleteFileA(rotated.c_str());
        MoveFileA(path.c_str(), rotated.c_str());
        handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }
    }

    std::string record = line + "\r\n";
    SetFilePointer(handle, 0, NULL, FILE_END);
    DWORD written = 0;
    WriteFile(handle, record.data(), static_cast<DWORD>(record.size()),
              &written, NULL);
    CloseHandle(handle);
}

}  // namespace

struct Logger::Impl {
    std::mutex mtx;  // 保护本实例全部成员；与 g_write_mutex 无嵌套顺序。
    LogLevel level = LogLevel::Info;
    std::vector<std::function<void(const std::string&)>> sinks;
    bool file_enabled = false;
    std::string directory;
};

Logger::Logger(LogLevel level) : impl_(std::make_unique<Impl>()) {
    impl_->level = level;
}

Logger::~Logger() = default;

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->level = level;
}

LogLevel Logger::Level() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->level;
}

void Logger::SetFileSink(bool enabled, const std::string& directory) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->file_enabled = enabled;
    impl_->directory = directory;
}

void Logger::AddSink(std::function<void(const std::string&)> sink) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->sinks.push_back(std::move(sink));
}

void Logger::LogImpl(LogLevel level, const std::string& message) {
    std::string line;
    bool file_enabled = false;
    std::string directory;
    std::vector<std::function<void(const std::string&)>> sinks;
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        if (level < impl_->level) {
            return;
        }
        line = CurrentTimestamp() + " [" + LevelName(level) + "] " + message;
        file_enabled = impl_->file_enabled;
        directory = impl_->directory;
        sinks = impl_->sinks;
    }
    std::lock_guard<std::mutex> lock(g_write_mutex);
    for (const auto& sink : sinks) {
        sink(line);
    }
    if (file_enabled) {
        AppendToFile(directory, line);
    }
}

void Logger::Debug(const std::string& message) {
    LogImpl(LogLevel::Debug, message);
}

void Logger::Info(const std::string& message) {
    LogImpl(LogLevel::Info, message);
}

void Logger::Warn(const std::string& message) {
    LogImpl(LogLevel::Warn, message);
}

void Logger::Error(const std::string& message) {
    LogImpl(LogLevel::Error, message);
}

std::string Logger::Format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

}  // namespace lamp
