#pragma once

#include <string>

class Engine {
public:
    void Init(const std::string& ProjectRoot);

    void Shutdown();
};
