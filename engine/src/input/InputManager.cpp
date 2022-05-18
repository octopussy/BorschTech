#include <cassert>
#include <string>
#include "InputManager.h"
#include "Core/Logging.h"

namespace bt::input {

    void InputManager::Init() {
        RAWINPUTDEVICE Rid[2] = {};

        // Register mouse:
        Rid[0].usUsagePage = 0x01;
        Rid[0].usUsage = 0x02;
        Rid[0].dwFlags = 0;
        Rid[0].hwndTarget = nullptr;

        // Register keyboard:
        Rid[1].usUsagePage = 0x01;
        Rid[1].usUsage = 0x06;
        Rid[1].dwFlags = 0;
        Rid[1].hwndTarget = nullptr;

        if (RegisterRawInputDevices(Rid, sizeof(Rid) / sizeof(RAWINPUTDEVICE), sizeof(Rid[0])) == FALSE) {
            //registration failed. Call GetLastError for the cause of the error
            assert(0);
        }
    }

    void InputManager::ParseMessage(void *lparam) {
        UINT size;
        UINT result;

        result = GetRawInputData((HRAWINPUT)lparam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        assert(result == 0);

        if (size > 0)
        {
            RAWINPUT input;
            result = GetRawInputData((HRAWINPUT)lparam, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));
            if (result == size)
            {
                mInputMessages.push_back(input);
            }
        }
    }

    void InputManager::Update() {
        for (auto& state: mKeysState) {
            state.IsJustPressed = false;
            state.IsJustReleased = false;
        }

        for (auto& msg: mInputMessages) {
            HandleRawInput(msg);
        }

        mInputMessages.clear();

        while (true) {
            UINT rawBufferSize = 0;
            UINT count = GetRawInputBuffer(nullptr, &rawBufferSize, sizeof(RAWINPUTHEADER));
            assert(count == 0);
            rawBufferSize *= 8;
            if (rawBufferSize == 0) {
                // Usually we should exit here (no more inputs in buffer):
                //allocator.reset();
                return;
            }

            // Fill up buffer:
            /*auto rawBuffer = (PRAWINPUT) allocator.allocate((size_t) rawBufferSize);
            if (rawBuffer == nullptr) {
                assert(0);
                //allocator.reset();
                return;
            }*/
            count = GetRawInputBuffer(mInputBuffer, &rawBufferSize, sizeof(RAWINPUTHEADER));
            if (count == -1) {
                HRESULT error = HRESULT_FROM_WIN32(GetLastError());
                assert(0);
                //allocator.reset();
                return;
            }

            // Process all the events:
            bt::log::Debug(std::to_string(count));
            for (UINT current_raw = 0; current_raw < count; ++current_raw) {
                HandleRawInput(mInputBuffer[current_raw]);
            }
        }
    }

    void InputManager::HandleRawInput(const RAWINPUT &raw) {
        if (raw.header.dwType == RIM_TYPEKEYBOARD) {
            const RAWKEYBOARD &rawkeyboard = raw.data.keyboard;
            auto& state = mKeysState[rawkeyboard.VKey];

            auto wasPressed = state.IsPressed;

            state.IsPressed = rawkeyboard.Flags == RI_KEY_MAKE;
            state.IsJustPressed = !wasPressed;
            state.IsJustReleased = wasPressed && !state.IsPressed;
        }

    }
}
