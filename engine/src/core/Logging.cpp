#include <Windows.h>
#include <iostream>

#include "Logging.h"
#include "Application.h"

namespace bt::log {

    std::unique_ptr<class Logger> GLogger;

    void Logger::Append(LogLevel level, const std::string &msg) {
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
