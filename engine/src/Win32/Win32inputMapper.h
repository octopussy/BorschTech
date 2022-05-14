#pragma once

#include "WinHPreface.h"
#include <Windows.h>
#include "WinHPostface.h"

#include "../Input/InputManager.h"

bt::InputKey MapWin32InputKey(int Key) {
    return bt::A;
}
