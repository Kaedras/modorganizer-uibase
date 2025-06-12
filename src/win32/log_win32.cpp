#include "../pch.h"
#include "log.h"

#pragma warning(push)
#pragma warning(disable : 4668)
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4365)
#define SPDLOG_WCHAR_FILENAMES 1
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#pragma warning(pop)

namespace MOBase::log
{

void Logger::createLogger(const std::string& name)
{
  m_sinks.reset(new spdlog::sinks::dist_sink<std::mutex>);

  DWORD console_mode;
  if (::GetConsoleMode(::GetStdHandle(STD_ERROR_HANDLE), &console_mode) != 0) {
    using sink_type = spdlog::sinks::wincolor_stderr_sink_mt;
    m_console.reset(new sink_type);

    if (auto* cs = dynamic_cast<sink_type*>(m_console.get())) {
      cs->set_color(spdlog::level::info,
                    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
      cs->set_color(spdlog::level::debug,
                    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    addSink(m_console);
  }

  m_logger.reset(new spdlog::logger(name, m_sinks));
}

}  // namespace MOBase::log
