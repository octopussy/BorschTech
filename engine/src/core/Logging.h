#pragma once

#include <memory>
#include <vector>

namespace bt::log {

    extern std::unique_ptr<class Logger> GLogger;

    enum LogLevel : uint8_t {
        INFO, DEBUG, WARNING, ERR
    };

    struct LogLine {
        LogLevel Level;
        std::string Message;
    };

    class ILogDelegate {
    public:
        virtual ~ILogDelegate() = default;
        virtual void Append(LogLevel level, const std::string &msg) = 0;
    };

    class Logger {
    public:
        void Append(LogLevel level, const std::string &msg);
        void RegisterDelegate(ILogDelegate* del) { m_delegates.push_back(del); }
    private:
        std::vector<LogLine> m_lines;
        std::vector<ILogDelegate*> m_delegates;
    };

    void Log(LogLevel level, const std::string &msg);

    void Info(const std::string &msg);

    void Debug(const std::string &msg);

    void Warning(const std::string &msg);

    void Error(const std::string &msg);

}
