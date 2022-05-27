#include <Windows.h>
#include <memory>

#include "Engine.h"
#include "Application.h"
#include "Timer.hpp"

LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam);

using namespace bt;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow) {
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

//return run_vulkan_imgui_test();
//return run_win31_dx12_test();

    GEngine = std::make_unique<Engine>();
    GEngine->Init("d:/_borsch_project", "d:/BorschTech/3rdparty/daScript");

    gTheApp = std::make_unique<Application>();


  /*  const auto *cmdLine = GetCommandLineA();
    if (!gTheApp->ProcessCommandLine(cmdLine))
        return -1;*/
/*
    std::wstring Title(L"Tutorial00: Hello Win32");
    switch (gTheApp->GetDeviceType()) {
        case RENDER_DEVICE_TYPE_D3D11:
            Title.append(L" (D3D11)");
            break;
        case RENDER_DEVICE_TYPE_D3D12:
            Title.append(L" (D3D12)");
            break;
        case RENDER_DEVICE_TYPE_GL:
            Title.append(L" (GL)");
            break;
        case RENDER_DEVICE_TYPE_VULKAN:
            Title.append(L" (VK)");
            break;
    }*/

    std::wstring WindowClass(L"BorschWindow");
    std::wstring WindowTitle(L"BorschTech");

// Register our window class
    WNDCLASSEX wcex = {
            sizeof(WNDCLASSEX), CS_CLASSDC, MessageProc,
            0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, WindowClass.c_str(), nullptr
    };
    RegisterClassEx(&wcex);

// Create a window
    const LONG WindowWidth = 1280;
    const LONG WindowHeight = 1024;
    RECT Rc = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&Rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(WindowClass.c_str(), WindowTitle.c_str(),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            Rc.right - Rc.left, Rc.bottom - Rc.top, nullptr, nullptr, wcex.hInstance, nullptr);
    if (!wnd) {
        MessageBox(nullptr, L"Cannot create window", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, SW_SHOWDEFAULT);
    UpdateWindow(wnd);

    if (!bt::gTheApp->Init(wnd))
        return -1;

    Diligent::Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();

// Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            auto CurrTime = Timer.GetElapsedTime();
            auto ElapsedTime = CurrTime - PrevTime;
            PrevTime = CurrTime;
            bt::gTheApp->Tick(CurrTime, ElapsedTime);
        }
    }

    bt::gTheApp->Shutdown();
    bt::GEngine->Shutdown();

    bt::gTheApp.reset();

    return static_cast<int>(msg.wParam);
}

// Called every time the NativeNativeAppBase receives a message
LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (bt::gTheApp) {
        auto res = bt::gTheApp->HandleWin32Message(wnd, message, wParam, lParam);
        if (res != 0)
            return res;
    }

    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(wnd, &ps);
            EndPaint(wnd, &ps);
            return 0;
        }
        case WM_SIZE: // Window size has been changed
            if (bt::gTheApp) {
                bt::gTheApp->WindowResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_CHAR:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO) lParam;

            lpMMI->ptMinTrackSize.x = 320;
            lpMMI->ptMinTrackSize.y = 240;
            return 0;
        }

        default:
            return DefWindowProc(wnd, message, wParam, lParam);
    }
}
