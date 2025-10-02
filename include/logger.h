#pragma once
#include <sstream>
#include <string>
#include <mutex>
#include <ostream>
#include <memory>
#include <iostream>

enum class LogLevel {TRACE, DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    // create with program wide name and option output stream 
    explicit Logger(std::string name, std::ostream& out = std::cout);

    // non-copyyable, movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) noexcept;
    Logger& operator=(Logger&&) noexcept;

    void set_level(LogLevel level) noexcept;
    LogLevel level() const noexcept;


    // Basic loging API (thread-safe)
    void log(LogLevel lvl, const std::string& msg);
    void trace(const std::string& msg);
    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

    // convenience: formated log 
    template<typename... Args>
    void info_fmt(const std::string& fmt, Args&&... args);

private:
    std::string name_;
    LogLevel level_;
    std::ostream* out_;
    std::unique_ptr<std::mutex> mutex_;
    void emit(LogLevel lvl, const std::string& payload);

};

template<typename... Args>
inline void Logger::info_fmt(const std::string& /*fmt*/, Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    info(oss.str());
}







