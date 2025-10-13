#include "Logger.h"
#include <iostream>
#include <iomanip>

namespace AgenticFramework {

std::unique_ptr<Logger> Logger::instance_;
std::once_flag Logger::init_flag_;

// ConsoleSink implementation
void ConsoleSink::write(const LogEntry& entry) {
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream thread_id_str;
    thread_id_str << entry.thread_id;
    
    std::cout << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count()
              << " [" << to_string(entry.level) << "]"
              << " [" << thread_id_str.str().substr(0, 8) << "]";
    
    if (!entry.context.empty()) {
        std::cout << " [" << entry.context << "]";
    }
    
    std::cout << " " << entry.message;
    
    if (!entry.file.empty()) {
        // Extract just the filename from the full path
        size_t pos = entry.file.find_last_of("/\\");
        std::string filename = (pos != std::string::npos) ? entry.file.substr(pos + 1) : entry.file;
        std::cout << " (" << filename << ":" << entry.line << ")";
    }
    
    std::cout << std::endl;
}

void ConsoleSink::flush() {
    std::cout.flush();
}

// FileSink implementation
FileSink::FileSink(const std::string& filename) : file_(filename, std::ios::app) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }
}

FileSink::~FileSink() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileSink::write(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream thread_id_str;
    thread_id_str << entry.thread_id;
    
    file_ << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
          << "." << std::setfill('0') << std::setw(3) << ms.count()
          << " [" << to_string(entry.level) << "]"
          << " [" << thread_id_str.str().substr(0, 8) << "]";
    
    if (!entry.context.empty()) {
        file_ << " [" << entry.context << "]";
    }
    
    file_ << " " << entry.message;
    
    if (!entry.file.empty()) {
        size_t pos = entry.file.find_last_of("/\\");
        std::string filename = (pos != std::string::npos) ? entry.file.substr(pos + 1) : entry.file;
        file_ << " (" << filename << ":" << entry.line << ")";
    }
    
    file_ << std::endl;
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.flush();
}

// Logger implementation
Logger& Logger::getInstance() {
    std::call_once(init_flag_, []() {
        instance_ = std::unique_ptr<Logger>(new Logger());
        // Add default console sink
        instance_->addSink(std::make_unique<ConsoleSink>());
    });
    return *instance_;
}

void Logger::addSink(std::unique_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::setMinLevel(LogLevel level) noexcept {
    min_level_.store(level);
}

LogLevel Logger::getMinLevel() const noexcept {
    return min_level_.load();
}

void Logger::log(LogLevel level, std::string_view message, 
                 std::string_view context,
                 std::string_view file, int line, 
                 std::string_view function) {
    if (level < min_level_.load()) {
        return;
    }
    
    LogEntry entry(level, std::string{message}, std::string{context}, 
                   std::string{file}, line, std::string{function});
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->write(entry);
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

} // namespace AgenticFramework