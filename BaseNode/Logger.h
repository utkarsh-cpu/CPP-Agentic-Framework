#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <vector>
#include <unordered_map>

namespace AgenticFramework {

// Log severity levels
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

// Convert log level to string
[[nodiscard]] inline const char* to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// Log entry structure
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
    std::string context;
    std::thread::id thread_id;
    std::string file;
    int line;
    std::string function;
    
    LogEntry(LogLevel lvl, std::string msg, std::string ctx = "",
             std::string f = "", int l = 0, std::string func = "")
        : timestamp(std::chrono::system_clock::now())
        , level(lvl)
        , message(std::move(msg))
        , context(std::move(ctx))
        , thread_id(std::this_thread::get_id())
        , file(std::move(f))
        , line(l)
        , function(std::move(func)) {}
};

// Abstract log sink interface
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

// Console log sink
class ConsoleSink : public LogSink {
public:
    void write(const LogEntry& entry) override;
    void flush() override;
};

// File log sink
class FileSink : public LogSink {
private:
    std::ofstream file_;
    mutable std::mutex mutex_;
    
public:
    explicit FileSink(const std::string& filename);
    ~FileSink();
    
    void write(const LogEntry& entry) override;
    void flush() override;
};

// Main logger class
class Logger {
private:
    std::vector<std::unique_ptr<LogSink>> sinks_;
    std::atomic<LogLevel> min_level_{LogLevel::INFO};
    mutable std::mutex mutex_;
    
    static std::unique_ptr<Logger> instance_;
    static std::once_flag init_flag_;
    
    Logger() = default;
    
public:
    static Logger& getInstance();
    
    void addSink(std::unique_ptr<LogSink> sink);
    void setMinLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel getMinLevel() const noexcept;
    
    void log(LogLevel level, std::string_view message, 
             std::string_view context = "",
             std::string_view file = "", int line = 0, 
             std::string_view function = "");
    
    // Perfect forwarding log method for efficient string construction
    template<typename... Args>
    void log_formatted(LogLevel level, std::string_view format, Args&&... args) {
        if (level < min_level_.load()) {
            return;
        }
        // Simple implementation - in production you'd want proper formatting
        (void)sizeof...(args); // Suppress unused parameter warning
        log(level, std::string{format});
    }
    
    // Simple logging without format strings for now
    void logMessage(LogLevel level, std::string_view message) {
        log(level, message);
    }
    
    void flush();
    
private:
};

} // namespace AgenticFramework

// Convenience macros for logging
#define LOG_TRACE(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::TRACE, msg, "", __FILE__, __LINE__, __FUNCTION__)

#define LOG_DEBUG(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::DEBUG, msg, "", __FILE__, __LINE__, __FUNCTION__)

#define LOG_INFO(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::INFO, msg, "", __FILE__, __LINE__, __FUNCTION__)

#define LOG_WARN(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::WARN, msg, "", __FILE__, __LINE__, __FUNCTION__)

#define LOG_ERROR(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::ERROR, msg, "", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FATAL(msg) \
    AgenticFramework::Logger::getInstance().log(AgenticFramework::LogLevel::FATAL, msg, "", __FILE__, __LINE__, __FUNCTION__)

// Context-aware logging
#define LOG_CONTEXT(context, level, msg) \
    AgenticFramework::Logger::getInstance().log(level, msg, context, __FILE__, __LINE__, __FUNCTION__)