#pragma once

namespace bt::log {

    void Info(const std::string &msg) {
        OutputDebugStringA(msg.c_str());
        std::cout << "[INFO] " << msg << std::endl;
    }

    void Debug(const std::string &msg) {
        OutputDebugStringA(msg.c_str());
        std::cout << "[DEBUG] " << msg << std::endl;
    }

    void Warning(const std::string &msg) {
        OutputDebugStringA(msg.c_str());
        std::cout << "[WARNING] " << msg << std::endl;
    }

    void Error(const std::string &msg) {
        OutputDebugStringA(msg.c_str());
        std::cerr << "[ERROR] " << msg << std::endl;
    }

}
