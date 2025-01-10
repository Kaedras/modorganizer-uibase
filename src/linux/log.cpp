#include "log.h"
#include "../pch.h"
#include <iostream>

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

static const spdlog::string_view_t FOREGROUND_COLOR_WHITE = "\033[37m";

namespace MOBase::log
{

void Logger::createLogger(const std::string& name)
{
  m_sinks.reset(new spdlog::sinks::dist_sink<std::mutex>);

  using sink_type = spdlog::sinks::stderr_color_sink_mt;
  m_console.reset(new sink_type);

  if (auto* cs = dynamic_cast<sink_type*>(m_console.get())) {
    cs->set_color(spdlog::level::info, FOREGROUND_COLOR_WHITE);
    cs->set_color(spdlog::level::debug, FOREGROUND_COLOR_WHITE);
  }
  addSink(m_console);

  m_logger.reset(new spdlog::logger(name, m_sinks));
}

} // namespace MOBase::log