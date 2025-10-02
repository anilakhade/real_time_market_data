#include "logger.h"
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <iostream>
#include <ctime>

Logger::Logger(std::string name, std::ostream& out)
  : name_(std::move(name)),
    level_(LogLevel::INFO),
    out_(&out),
    mutex_(std::make_unique<std::mutex>())
{}

Logger::Logger(Logger&& other) noexcept
  : name_(std::move(other.name_)),
    level_(other.level_),
    out_(other.out_),
    mutex_(std::move(other.mutex_))
{}

Logger& Logger::operator=(Logger&& other) noexcept {
    if (this == &other) return *this;
    std::lock_guard<std::mutex> l1(*mutex_);
    std::lock_guard<std::mutex> l2(*other.mutex_);
    name_ = std::move(other.name_);
    level_ = other.level_;
    out_ = other.out_;
    mutex_ = std::move(other.mutex_);
    return *this;
}

void Logger::set_level(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lk(*mutex_);
    level_ = level;
}

LogLevel Logger::level() const noexcept {
    return level_;
}

void Logger::emit(LogLevel lvl, const std::string& payload) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_r(&t, &tm);
    std::ostringstream header;
    header << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setw(3) << std::setfill('0') << ms.count()
           << " [";

    switch (lvl) {
        case LogLevel::TRACE: header << "TRACE"; break;
        case LogLevel::DEBUG: header << "DEBUG"; break;
        case LogLevel::INFO:  header << "INFO";  break;
        case LogLevel::WARN:  header << "WARN";  break;
        case LogLevel::ERROR: header << "ERROR"; break;
    }
    header << "] " << name_ << ": ";

    std::lock_guard<std::mutex> lk(*mutex_);
    (*out_) << header.str() << payload << '\n';
    out_->flush();
}

void Logger::log(LogLevel lvl, const std::string& msg) {
    // simple level check
    if (static_cast<int>(lvl) < static_cast<int>(level_)) return;
    emit(lvl, msg);
}

void Logger::trace(const std::string& msg) { log(LogLevel::TRACE, msg); }
void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info(const std::string& msg)  { log(LogLevel::INFO,  msg); }
void Logger::warn(const std::string& msg)  { log(LogLevel::WARN,  msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }















