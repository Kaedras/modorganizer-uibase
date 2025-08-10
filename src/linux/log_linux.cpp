#include "../pch.h"
#include "log.h"
#include "utility.h"

#include <algorithm>
#include <locale>

#include <spdlog/logger.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/dist_sink.h>

static constexpr spdlog::string_view_t FOREGROUND_WHITE = "\033[37m";

namespace MOBase::log
{

void Logger::createLogger(const std::string& name)
{
  m_sinks.reset(new spdlog::sinks::dist_sink<std::mutex>);

  using sink_type = spdlog::sinks::ansicolor_stderr_sink_mt;
  m_console.reset(new sink_type);

  if (auto* cs = dynamic_cast<sink_type*>(m_console.get())) {
    cs->set_color(spdlog::level::info, FOREGROUND_WHITE);
    cs->set_color(spdlog::level::debug, FOREGROUND_WHITE);
  }
  addSink(m_console);

  m_logger.reset(new spdlog::logger(name, m_sinks));
}

}  // namespace MOBase::log
