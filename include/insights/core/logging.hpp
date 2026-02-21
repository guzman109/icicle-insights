#pragma once
#include "insights/core/config.hpp"

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include <filesystem>
#include <memory>
#include <spdlog/common.h>
#include <string_view>
#include <vector>

namespace insights::core {

// Shared console sink — all loggers write to the same stdout stream
inline std::shared_ptr<spdlog::sinks::stdout_color_sink_mt>
consoleSink() {
  static auto Sink =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  return Sink;
}

// Build a logger with console output and an optional rotating file.
// The file is placed at {LogDir}/{name}.log when LogDir is set.
// The logger is registered in spdlog's global registry so any translation
// unit can retrieve it with spdlog::get(name).
inline void createLogger(std::string_view Name, const Config &Cfg) {
  std::vector<spdlog::sink_ptr> Sinks{consoleSink()};

  if (Cfg.LogDir) {
    std::filesystem::path Dir{*Cfg.LogDir};
    std::filesystem::create_directories(Dir);
    Sinks.push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            Dir / (std::string(Name) + ".log"), 1024 * 1024 * 10, 3
        )
    );
  }

  auto Logger = std::make_shared<spdlog::logger>(
      std::string(Name), Sinks.begin(), Sinks.end()
  );
  Logger->set_level(spdlog::level::from_str(Cfg.LogLevel));
  spdlog::register_logger(Logger);
}

inline void setupLogging(const Config &Cfg) {
  // Server logger becomes the default — all bare spdlog::info() calls use it
  createLogger("server", Cfg);
  spdlog::set_default_logger(spdlog::get("server"));

  // Flush errors immediately; flush info-level logs every second
  spdlog::flush_on(spdlog::level::warn);
  spdlog::flush_every(std::chrono::seconds(1));
}

} // namespace insights::core
