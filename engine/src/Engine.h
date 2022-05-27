#pragma once

#include <string>
#include <memory>

#include "input/InputManager.h"

using namespace std;

namespace bt {

    extern std::unique_ptr<class Engine> GEngine;
    extern std::unique_ptr<input::InputManager> GInputManager;

    class Engine {
    public:
        void Init(const string &projectRoot, const string &dasRoot);
        void Shutdown();
    };
}
