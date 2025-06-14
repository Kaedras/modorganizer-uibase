#include "../pch.h"
#include "log.h"
#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

static constexpr spdlog::string_view_t FOREGROUND_COLOR_WHITE = "\033[37m";

namespace MOBase::log
{

void Logger::createLogger(const std::string& name)
{
  m_sinks = std::make_shared<spdlog::sinks::dist_sink<std::mutex>>();

  using sink_type = spdlog::sinks::ansicolor_stderr_sink_mt;
  m_console       = std::make_shared<sink_type>();

  if (auto* cs = dynamic_cast<sink_type*>(m_console.get())) {
    cs->set_color(spdlog::level::info, FOREGROUND_COLOR_WHITE);
    cs->set_color(spdlog::level::debug, FOREGROUND_COLOR_WHITE);
  }
  addSink(m_console);

  m_logger = std::make_unique<spdlog::logger>(name, m_sinks);
}

}  // namespace MOBase::log
