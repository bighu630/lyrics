#pragma once

#include <memory>
#include <string>

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace lyrics {

/// Initialize global spdlog loggers.
/// Two sinks: console (color) + file (/tmp/lyrics.log, rotating).
/// @param level  one of "trace","debug","info","warn","error","critical","off"
inline void init_logger(const std::string& level = "info") {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::string log_path = "/tmp/lyrics.log";
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path, 1024 * 1024 * 5, 3);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

    auto logger = std::make_shared<spdlog::logger>("lyrics", sinks.begin(), sinks.end());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

    if (level == "trace")        logger->set_level(spdlog::level::trace);
    else if (level == "debug")   logger->set_level(spdlog::level::debug);
    else if (level == "info")    logger->set_level(spdlog::level::info);
    else if (level == "warn")    logger->set_level(spdlog::level::warn);
    else if (level == "error")   logger->set_level(spdlog::level::err);
    else if (level == "critical")logger->set_level(spdlog::level::critical);
    else                         logger->set_level(spdlog::level::info);

    logger->flush_on(spdlog::level::trace);

    spdlog::set_default_logger(logger);
    spdlog::set_level(logger->level());
}

/// Convenience macros
#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

} // namespace lyrics
