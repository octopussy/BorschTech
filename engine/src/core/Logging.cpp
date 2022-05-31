#include "Logging.h"

#include <Windows.h>
#include <iostream>

#include "Application.h"

#include "spdlog/spdlog.h"
#include "spdlog/async.h" //support for async logging.
#include "spdlog/sinks/basic_file_sink.h"

namespace bt::log {

    std::unique_ptr<class Logger> GLogger;

    void Logger::Append(LogLevel level, const std::string &msg) {

        try
        {
            auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "logs/async_log.txt");
            for (int i = 1; i < 101; ++i)
            {
                async_file->info("Async message #{}", i);
            }
            // Under VisualStudio, this must be called before main finishes to workaround a known VS issue
            spdlog::drop_all();
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            std::cout << "Log initialization failed: " << ex.what() << std::endl;
        }

        std::string m;
        switch (level) {
            case INFO:
                m = "[INFO]" + msg;
                std::cout << m << std::endl;
                break;
            case DEBUG:
                m = "[DEBUG] " + msg;
                std::cout << m << std::endl;
                break;
            case WARNING:
                m = "[WARNING] " + msg;
                std::cout << m << std::endl;
                break;
            case ERR:
                m = "[ERROR] " + msg;
                std::cerr << m << std::endl;
                break;
        }
        OutputDebugStringA(m.c_str());
        m_lines.push_back(LogLine { level, msg });

        for (auto d: m_delegates) {
            d->Append(level, msg);
        }
    }

    void Log(LogLevel level, const std::string &msg) {
        if (GLogger) {
            GLogger->Append(level, msg);
        }
    }

    void Info(const std::string &msg) {
        Log(LogLevel::INFO, msg);
    }

    void Debug(const std::string &msg) {
        Log(LogLevel::DEBUG, msg);
    }

    void Warning(const std::string &msg) {
        Log(LogLevel::WARNING, msg);
    }

    void Error(const std::string &msg) {
        Log(LogLevel::ERR, msg);
    }
}
