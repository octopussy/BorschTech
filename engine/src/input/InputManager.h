#pragma once

#include <vector>
#include <Windows.h>

namespace bt::input {

    enum Key {
        A = 65,
        B,
        C,
        D,
        E
    };

    struct KeyState {
        int IsPressed: 1;
        int IsJustPressed: 1;
        int IsJustReleased: 1;
    };

    class InputManager {

        RAWINPUT mInputBuffer[1024];

        std::vector<RAWINPUT> mInputMessages;

        KeyState mKeysState[512];

    public:
        void Init();

        void ParseMessage(void *lparam);

        void Update();

        bool IsKeyPressed(Key Key) {
            const auto& State = mKeysState[Key];
            return State.IsPressed;
        }

        bool IsKeyJustPressed(Key Key) {
            const auto& State = mKeysState[Key];
            return State.IsJustPressed;
        }

        bool IsKeyJustReleased(Key Key) {
            const auto& State = mKeysState[Key];
            return State.IsJustReleased;
        }

    private:

        void HandleRawInput(const RAWINPUT &raw);

    };

}
