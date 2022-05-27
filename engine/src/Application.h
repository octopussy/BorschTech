#pragma once

#ifndef NOMINMAX
#    define NOMINMAX
#endif

#include <Windows.h>

#ifndef PLATFORM_WIN32
#    define PLATFORM_WIN32 1
#endif

#ifndef ENGINE_DLL
#    define ENGINE_DLL 1
#endif

#ifndef D3D11_SUPPORTED
#    define D3D11_SUPPORTED 1
#endif

#ifndef D3D12_SUPPORTED
#    define D3D12_SUPPORTED 1
#endif

#ifndef GL_SUPPORTED
#    define GL_SUPPORTED 1
#endif

#ifndef VULKAN_SUPPORTED
#    define VULKAN_SUPPORTED 1
#endif

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "ImGuiImpl.hpp"

#include "Engine.h"
#include "core/Math.h"
#include "Camera.h"
#include "input/InputManager.h"
#include "editor/RenderTarget.h"
#include "core/Logging.h"
#include "imgui.h"

using namespace Diligent;

class EditorLog : public bt::log::ILogDelegate
{
public:
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    bool                AutoScroll;  // Keep scrolling if already at the bottom.

    explicit EditorLog()
    {
        AutoScroll = true;
        Clear();
    }

    void Append(bt::log::LogLevel level, const string &msg) override;

    void Clear()
    {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back(0);
    }

    void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        int old_size = Buf.size();
        va_list args;
                va_start(args, fmt);
        Buf.appendfv(fmt, args);
                va_end(args);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size + 1);
    }

    void    Draw(const char* title, bool* p_open = NULL)
    {
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -100.0f);

        ImGui::Separator();
        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if (Filter.IsActive())
        {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have a random access on the result on our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of
            // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                if (Filter.PassFilter(line_start, line_end))
                    ImGui::TextUnformatted(line_start, line_end);
            }
        }
        else
        {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
            // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
            // within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
            // on your side is recommended. Using ImGuiListClipper requires
            // - A) random access into your data
            // - B) items all being the  same height,
            // both of which we can handle since we an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display
            // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
            // it possible (and would be recommended if you want to search through tens of thousands of entries).
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    ImGui::TextUnformatted(line_start, line_end);
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
};

namespace bt {

    extern std::unique_ptr<class Application> gTheApp;

    class TestCube;

    class Application {
    public:
        IEngineFactory *GetEngineFactory() { return m_pEngineFactory.RawPtr(); }

        IRenderDevice *GetRenderDevice() { return m_pDevice.RawPtr(); }

        ISwapChain *GetSwapChain() { return m_pSwapChain.RawPtr(); }

        IDeviceContext *GetImmediateContext() { return m_pImmediateContext.RawPtr(); }

        bool CreateSwapChain(RefCntAutoPtr<ISwapChain> &result, HWND hWnd, bool isAdditional);

        Application();

        virtual ~Application();

        bool Init(HWND hWnd);

        void Shutdown();

        void Tick(double CurrTime, double ElapsedTime);

        void WindowResize(Uint32 Width, Uint32 Height);

        virtual LRESULT HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        void Update(double CurrTime, double ElapsedTime);

        void PrepareRender();

        void Present();

        void Render();

        void DrawImGui();

    private:

        RefCntAutoPtr<IEngineFactory> m_pEngineFactory;
        RefCntAutoPtr<IRenderDevice> m_pDevice;
        RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
        //std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeviceContexts;
        RefCntAutoPtr<ISwapChain> m_pSwapChain;
        RENDER_DEVICE_TYPE m_DeviceType = Diligent::RENDER_DEVICE_TYPE_VULKAN;

        std::unique_ptr<ImGuiImpl> m_pImGui;

        Camera mCamera;

        EditorLog* m_log;

    public:
        std::unique_ptr<RenderTarget> mTestRenderTarget;

        std::unique_ptr<TestCube> mCube;
        std::unique_ptr<TestCube> mCube2;
    };

}
