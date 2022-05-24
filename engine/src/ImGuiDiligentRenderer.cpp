/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <cstddef>

#include "Application.h"
#include "ImGuiDiligentRenderer.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "MapHelper.hpp"

#include "Core/Logging.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <tchar.h>
#include <dwmapi.h>

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD

#include <xinput.h>

typedef DWORD (WINAPI *PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES *);

typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD, XINPUT_STATE *);

#endif

#ifndef DPI_ENUMS_DECLARED
typedef enum {
  PROCESS_DPI_UNAWARE = 0, PROCESS_SYSTEM_DPI_AWARE = 1, PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum {
  MDT_EFFECTIVE_DPI = 0, MDT_ANGULAR_DPI = 1, MDT_RAW_DPI = 2, MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

using namespace Diligent;

class bt::ImGuiDiligentRenderer *RENDERER;

// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplWin32_ViewportData {
  HWND Hwnd;
  bool HwndOwned;
  DWORD DwStyle;
  DWORD DwExStyle;

  ImGui_ImplWin32_ViewportData() {
      Hwnd = NULL;
      HwndOwned = false;
      DwStyle = DwExStyle = 0;
  }

  ~ImGui_ImplWin32_ViewportData() { IM_ASSERT(Hwnd == NULL); }
};

struct ImGui_ImplWin32_Data {
  HWND hWnd;
  HWND MouseHwnd;
  bool MouseTracked;
  int MouseButtonsDown;
  INT64 Time;
  INT64 TicksPerSecond;
  ImGuiMouseCursor LastMouseCursor;
  bool HasGamepad;
  bool WantUpdateHasGamepad;
  bool WantUpdateMonitors;

#ifndef IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
  HMODULE XInputDLL;
  PFN_XInputGetCapabilities XInputGetCapabilities;
  PFN_XInputGetState XInputGetState;
#endif

  ImGui_ImplWin32_Data() { memset((void *) this, 0, sizeof(*this)); }
};

struct BorschDiligentRenderData {
};

struct BorschDiligentViewportData {
  RefCntAutoPtr<ISwapChain> pSwapChain;
  RefCntAutoPtr<IBuffer> m_pVB;
  RefCntAutoPtr<IBuffer> m_pIB;
  //RefCntAutoPtr<IPipelineState> m_pPSO;
  Uint32 m_VertexBufferSize = 0;
  Uint32 m_IndexBufferSize = 0;
  Int32 Width;
  Int32 Height;

  BorschDiligentViewportData() :
      m_VertexBufferSize{1024},
      m_IndexBufferSize{1024} {}

};

static void ImGui_ImplWin32_InitPlatformInterface();

static void ImGui_ImplDX12_InitPlatformInterface();

typedef HRESULT(WINAPI *PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT *, UINT *);

// Perform our own check with RtlVerifyVersionInfo() instead of using functions from <VersionHelpers.h> as they
// require a manifest to be functional for checks above 8.1. See https://github.com/ocornut/imgui/issues/4200
static BOOL _IsWindowsVersionOrGreater(WORD major, WORD minor, WORD) {
    typedef LONG(WINAPI *PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW *, ULONG, ULONGLONG);
    static PFN_RtlVerifyVersionInfo RtlVerifyVersionInfoFn = NULL;
    if (RtlVerifyVersionInfoFn == NULL)
        if (HMODULE ntdllModule = ::GetModuleHandleA("ntdll.dll"))
            RtlVerifyVersionInfoFn = (PFN_RtlVerifyVersionInfo) GetProcAddress(ntdllModule, "RtlVerifyVersionInfo");
    if (RtlVerifyVersionInfoFn == NULL)
        return FALSE;

    RTL_OSVERSIONINFOEXW versionInfo = {};
    ULONGLONG conditionMask = 0;
    versionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
    versionInfo.dwMajorVersion = major;
    versionInfo.dwMinorVersion = minor;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    return (RtlVerifyVersionInfoFn(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask) == 0) ? TRUE
                                                                                                           : FALSE;
}

#define _IsWindowsVistaOrGreater()   _IsWindowsVersionOrGreater(HIBYTE(0x0600), LOBYTE(0x0600), 0) // _WIN32_WINNT_VISTA
#define _IsWindows8OrGreater()       _IsWindowsVersionOrGreater(HIBYTE(0x0602), LOBYTE(0x0602), 0) // _WIN32_WINNT_WIN8
#define _IsWindows8Point1OrGreater() _IsWindowsVersionOrGreater(HIBYTE(0x0603), LOBYTE(0x0603), 0) // _WIN32_WINNT_WINBLUE
#define _IsWindows10OrGreater()      _IsWindowsVersionOrGreater(HIBYTE(0x0A00), LOBYTE(0x0A00), 0) // _WIN32_WINNT_WINTHRESHOLD / _WIN32_WINNT_WIN10

namespace bt {
using namespace Diligent;

static const char *VertexShaderHLSL = R"(
cbuffer Constants
{
    float4x4 ProjectionMatrix;
}

struct VSInput
{
    float2 pos : ATTRIB0;
    float2 uv  : ATTRIB1;
    float4 col : ATTRIB2;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.pos = mul(ProjectionMatrix, float4(VSIn.pos.xy, 0.0, 1.0));
    PSIn.col = VSIn.col;
    PSIn.uv  = VSIn.uv;
}
)";

static const char *PixelShaderHLSL = R"(
struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

Texture2D    Texture;
SamplerState Texture_sampler;

float4 main(in PSInput PSIn) : SV_Target
{
    return PSIn.col * Texture.Sample(Texture_sampler, PSIn.uv);
}
)";


static const char *VertexShaderGLSL = R"(
#ifdef VULKAN
#   define BINDING(X) layout(binding=X)
#   define OUT_LOCATION(X) layout(location=X) // Requires separable programs
#else
#   define BINDING(X)
#   define OUT_LOCATION(X)
#endif
BINDING(0) uniform Constants
{
    mat4 ProjectionMatrix;
};

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_col;

OUT_LOCATION(0) out vec4 vsout_col;
OUT_LOCATION(1) out vec2 vsout_uv;

#ifndef GL_ES
out gl_PerVertex
{
    vec4 gl_Position;
};
#endif

void main()
{
    gl_Position = ProjectionMatrix * vec4(in_pos.xy, 0.0, 1.0);
    vsout_col = in_col;
    vsout_uv  = in_uv;
}
)";

static const char *PixelShaderGLSL = R"(
#ifdef VULKAN
#   define BINDING(X) layout(binding=X)
#   define IN_LOCATION(X) layout(location=X) // Requires separable programs
#else
#   define BINDING(X)
#   define IN_LOCATION(X)
#endif
BINDING(0) uniform sampler2D Texture;

IN_LOCATION(0) in vec4 vsout_col;
IN_LOCATION(1) in vec2 vsout_uv;

layout(location = 0) out vec4 psout_col;

void main()
{
    psout_col = vsout_col * texture(Texture, vsout_uv);
}
)";


// clang-format off

// glslangValidator.exe -V -e main --vn VertexShader_SPIRV ImGUI.vert

static constexpr uint32_t VertexShader_SPIRV[] =
    {
        0x07230203, 0x00010000, 0x0008000a, 0x00000028, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
        0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
        0x000b000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000a, 0x00000016, 0x00000020,
        0x00000022, 0x00000025, 0x00000026, 0x00030003, 0x00000002, 0x000001a4, 0x00040005, 0x00000004,
        0x6e69616d, 0x00000000, 0x00060005, 0x00000008, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000,
        0x00060006, 0x00000008, 0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000000a,
        0x00000000, 0x00050005, 0x0000000e, 0x736e6f43, 0x746e6174, 0x00000073, 0x00080006, 0x0000000e,
        0x00000000, 0x6a6f7250, 0x69746365, 0x614d6e6f, 0x78697274, 0x00000000, 0x00030005, 0x00000010,
        0x00000000, 0x00040005, 0x00000016, 0x705f6e69, 0x0000736f, 0x00050005, 0x00000020, 0x756f7376,
        0x6f635f74, 0x0000006c, 0x00040005, 0x00000022, 0x635f6e69, 0x00006c6f, 0x00050005, 0x00000025,
        0x756f7376, 0x76755f74, 0x00000000, 0x00040005, 0x00000026, 0x755f6e69, 0x00000076, 0x00050048,
        0x00000008, 0x00000000, 0x0000000b, 0x00000000, 0x00030047, 0x00000008, 0x00000002, 0x00040048,
        0x0000000e, 0x00000000, 0x00000005, 0x00050048, 0x0000000e, 0x00000000, 0x00000023, 0x00000000,
        0x00050048, 0x0000000e, 0x00000000, 0x00000007, 0x00000010, 0x00030047, 0x0000000e, 0x00000002,
        0x00040047, 0x00000010, 0x00000022, 0x00000000, 0x00040047, 0x00000010, 0x00000021, 0x00000000,
        0x00040047, 0x00000016, 0x0000001e, 0x00000000, 0x00040047, 0x00000020, 0x0000001e, 0x00000000,
        0x00040047, 0x00000022, 0x0000001e, 0x00000002, 0x00040047, 0x00000025, 0x0000001e, 0x00000001,
        0x00040047, 0x00000026, 0x0000001e, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
        0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004,
        0x0003001e, 0x00000008, 0x00000007, 0x00040020, 0x00000009, 0x00000003, 0x00000008, 0x0004003b,
        0x00000009, 0x0000000a, 0x00000003, 0x00040015, 0x0000000b, 0x00000020, 0x00000001, 0x0004002b,
        0x0000000b, 0x0000000c, 0x00000000, 0x00040018, 0x0000000d, 0x00000007, 0x00000004, 0x0003001e,
        0x0000000e, 0x0000000d, 0x00040020, 0x0000000f, 0x00000002, 0x0000000e, 0x0004003b, 0x0000000f,
        0x00000010, 0x00000002, 0x00040020, 0x00000011, 0x00000002, 0x0000000d, 0x00040017, 0x00000014,
        0x00000006, 0x00000002, 0x00040020, 0x00000015, 0x00000001, 0x00000014, 0x0004003b, 0x00000015,
        0x00000016, 0x00000001, 0x0004002b, 0x00000006, 0x00000018, 0x00000000, 0x0004002b, 0x00000006,
        0x00000019, 0x3f800000, 0x00040020, 0x0000001e, 0x00000003, 0x00000007, 0x0004003b, 0x0000001e,
        0x00000020, 0x00000003, 0x00040020, 0x00000021, 0x00000001, 0x00000007, 0x0004003b, 0x00000021,
        0x00000022, 0x00000001, 0x00040020, 0x00000024, 0x00000003, 0x00000014, 0x0004003b, 0x00000024,
        0x00000025, 0x00000003, 0x0004003b, 0x00000015, 0x00000026, 0x00000001, 0x00050036, 0x00000002,
        0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000011, 0x00000012,
        0x00000010, 0x0000000c, 0x0004003d, 0x0000000d, 0x00000013, 0x00000012, 0x0004003d, 0x00000014,
        0x00000017, 0x00000016, 0x00050051, 0x00000006, 0x0000001a, 0x00000017, 0x00000000, 0x00050051,
        0x00000006, 0x0000001b, 0x00000017, 0x00000001, 0x00070050, 0x00000007, 0x0000001c, 0x0000001a,
        0x0000001b, 0x00000018, 0x00000019, 0x00050091, 0x00000007, 0x0000001d, 0x00000013, 0x0000001c,
        0x00050041, 0x0000001e, 0x0000001f, 0x0000000a, 0x0000000c, 0x0003003e, 0x0000001f, 0x0000001d,
        0x0004003d, 0x00000007, 0x00000023, 0x00000022, 0x0003003e, 0x00000020, 0x00000023, 0x0004003d,
        0x00000014, 0x00000027, 0x00000026, 0x0003003e, 0x00000025, 0x00000027, 0x000100fd, 0x00010038
    };

static constexpr uint32_t FragmentShader_SPIRV[] =
    {
        0x07230203, 0x00010000, 0x0008000a, 0x00000018, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
        0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
        0x0008000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000b, 0x00000014,
        0x00030010, 0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001a4, 0x00040005, 0x00000004,
        0x6e69616d, 0x00000000, 0x00050005, 0x00000009, 0x756f7370, 0x6f635f74, 0x0000006c, 0x00050005,
        0x0000000b, 0x756f7376, 0x6f635f74, 0x0000006c, 0x00040005, 0x00000010, 0x74786554, 0x00657275,
        0x00050005, 0x00000014, 0x756f7376, 0x76755f74, 0x00000000, 0x00040047, 0x00000009, 0x0000001e,
        0x00000000, 0x00040047, 0x0000000b, 0x0000001e, 0x00000000, 0x00040047, 0x00000010, 0x00000022,
        0x00000000, 0x00040047, 0x00000010, 0x00000021, 0x00000000, 0x00040047, 0x00000014, 0x0000001e,
        0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006,
        0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003,
        0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020, 0x0000000a, 0x00000001,
        0x00000007, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001, 0x00090019, 0x0000000d, 0x00000006,
        0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x0000000e,
        0x0000000d, 0x00040020, 0x0000000f, 0x00000000, 0x0000000e, 0x0004003b, 0x0000000f, 0x00000010,
        0x00000000, 0x00040017, 0x00000012, 0x00000006, 0x00000002, 0x00040020, 0x00000013, 0x00000001,
        0x00000012, 0x0004003b, 0x00000013, 0x00000014, 0x00000001, 0x00050036, 0x00000002, 0x00000004,
        0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000007, 0x0000000c, 0x0000000b,
        0x0004003d, 0x0000000e, 0x00000011, 0x00000010, 0x0004003d, 0x00000012, 0x00000015, 0x00000014,
        0x00050057, 0x00000007, 0x00000016, 0x00000011, 0x00000015, 0x00050085, 0x00000007, 0x00000017,
        0x0000000c, 0x00000016, 0x0003003e, 0x00000009, 0x00000017, 0x000100fd, 0x00010038
    };
// clang-format on


static const char *ShadersMSL = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VSConstants
{
    float4x4 ProjectionMatrix;
};

struct VSIn
{
    float2 pos [[attribute(0)]];
    float2 uv  [[attribute(1)]];
    float4 col [[attribute(2)]];
};

struct VSOut
{
    float4 col [[user(locn0)]];
    float2 uv  [[user(locn1)]];
    float4 pos [[position]];
};

vertex VSOut vs_main(VSIn in [[stage_in]], constant VSConstants& Constants [[buffer(0)]])
{
    VSOut out = {};
    out.pos = Constants.ProjectionMatrix * float4(in.pos, 0.0, 1.0);
    out.col = in.col;
    out.uv  = in.uv;
    return out;
}

struct PSOut
{
    float4 col [[color(0)]];
};

fragment PSOut ps_main(VSOut in [[stage_in]],
                       texture2d<float> Texture [[texture(0)]],
                       sampler Texture_sampler  [[sampler(0)]])
{
    PSOut out = {};
    out.col = in.col * Texture.sample(Texture_sampler, in.uv);
    return out;
}
)";

ImGuiDiligentRenderer::ImGuiDiligentRenderer(
    void *hwnd,
    IRenderDevice *pDevice,
    TEXTURE_FORMAT BackBufferFmt,
    TEXTURE_FORMAT DepthBufferFmt,
    Uint32 InitialVertexBufferSize,
    Uint32 InitialIndexBufferSize) :
// clang-format off
    m_pDevice{pDevice},
    m_BackBufferFmt{BackBufferFmt},
    m_DepthBufferFmt{DepthBufferFmt},
    m_VertexBufferSize{InitialVertexBufferSize},
    m_IndexBufferSize{InitialIndexBufferSize}
// clang-format on
{

    RENDERER = this;
    //Check support vertex offset
    m_BaseVertexSupported = pDevice->GetAdapterInfo().DrawCommand.CapFlags & DRAW_COMMAND_CAP_FLAG_BASE_VERTEX;

    // Setup back-end capabilities flags
    IMGUI_CHECKVERSION();
    ImGuiIO &IO = ImGui::GetIO();

    IO.BackendRendererName = "ImGuiDiligentRenderer";

    INT64 perf_frequency, perf_counter;
    ::QueryPerformanceFrequency((LARGE_INTEGER *) &perf_frequency);
    ::QueryPerformanceCounter((LARGE_INTEGER *) &perf_counter);

    ImGui_ImplWin32_Data *platformData = IM_NEW(ImGui_ImplWin32_Data)();
    IO.BackendPlatformUserData = (void *) platformData;
    IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
    IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
    IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)
    IO.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can call io.AddMouseViewportEvent() with correct data (optional)
    if (m_BaseVertexSupported)
        IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    ImGuiIO &io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

    platformData->hWnd = (HWND) hwnd;
    platformData->WantUpdateHasGamepad = true;
    platformData->WantUpdateMonitors = true;
    platformData->TicksPerSecond = perf_frequency;
    platformData->Time = perf_counter;
    platformData->LastMouseCursor = ImGuiMouseCursor_COUNT;

    ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = main_viewport->PlatformHandleRaw = (void *) platformData->hWnd;
    if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplWin32_InitPlatformInterface();


    BorschDiligentRenderData *bd = IM_NEW(BorschDiligentRenderData)();
    IO.BackendRendererUserData = (void *) bd;
    IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplDX12_InitPlatformInterface();

    CreateDeviceObjects();
}

ImGuiDiligentRenderer::~ImGuiDiligentRenderer() {
}

void ImGuiDiligentRenderer::NewFrame(Uint32 RenderSurfaceWidth,
                                     Uint32 RenderSurfaceHeight,
                                     SURFACE_TRANSFORM SurfacePreTransform) {
    if (!m_pPSO)
        CreateDeviceObjects();
    m_RenderSurfaceWidth = RenderSurfaceWidth;
    m_RenderSurfaceHeight = RenderSurfaceHeight;
    m_SurfacePreTransform = SurfacePreTransform;
}

void ImGuiDiligentRenderer::EndFrame() {
}

void ImGuiDiligentRenderer::InvalidateDeviceObjects() {
    m_pVB.Release();
    m_pIB.Release();
    m_pVertexConstantBuffer.Release();
    m_pPSO.Release();
    m_pFontSRV.Release();
    m_pSRB.Release();
}

void ImGuiDiligentRenderer::CreateDeviceObjects() {
    InvalidateDeviceObjects();

    ShaderCreateInfo ShaderCI;
    ShaderCI.UseCombinedTextureSamplers = true;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_DEFAULT;

    const auto DeviceType = m_pDevice->GetDeviceInfo().Type;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.Desc.Name = "Imgui VS";
        switch (DeviceType) {
            case RENDER_DEVICE_TYPE_VULKAN:
                ShaderCI.ByteCode = VertexShader_SPIRV;
                ShaderCI.ByteCodeSize = sizeof(VertexShader_SPIRV);
                break;

            case RENDER_DEVICE_TYPE_D3D11:
            case RENDER_DEVICE_TYPE_D3D12:
                ShaderCI.Source = VertexShaderHLSL;
                break;

            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                ShaderCI.Source = VertexShaderGLSL;
                break;

            case RENDER_DEVICE_TYPE_METAL:
                ShaderCI.Source = ShadersMSL;
                ShaderCI.EntryPoint = "vs_main";
                break;

            default:
                    UNEXPECTED("Unknown render device type");
        }
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.Desc.Name = "Imgui PS";
        switch (DeviceType) {
            case RENDER_DEVICE_TYPE_VULKAN:
                ShaderCI.ByteCode = FragmentShader_SPIRV;
                ShaderCI.ByteCodeSize = sizeof(FragmentShader_SPIRV);
                break;

            case RENDER_DEVICE_TYPE_D3D11:
            case RENDER_DEVICE_TYPE_D3D12:
                ShaderCI.Source = PixelShaderHLSL;
                break;

            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
                ShaderCI.Source = PixelShaderGLSL;
                break;

            case RENDER_DEVICE_TYPE_METAL:
                ShaderCI.Source = ShadersMSL;
                ShaderCI.EntryPoint = "ps_main";
                break;

            default:
                    UNEXPECTED("Unknown render device type");
        }
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "ImGUI PSO";
    auto &GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    GraphicsPipeline.NumRenderTargets = 1;
    GraphicsPipeline.RTVFormats[0] = m_BackBufferFmt;
    GraphicsPipeline.DSVFormat = m_DepthBufferFmt;
    GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    GraphicsPipeline.RasterizerDesc.ScissorEnable = True;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    auto &RT0 = GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = True;
    RT0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOp = BLEND_OPERATION_ADD;
    RT0.SrcBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.DestBlendAlpha = BLEND_FACTOR_ZERO;
    RT0.BlendOpAlpha = BLEND_OPERATION_ADD;
    RT0.RenderTargetWriteMask = COLOR_MASK_ALL;

    LayoutElement VSInputs[] //
        {
            {0, 0, 2, VT_FLOAT32}, // pos
            {1, 0, 2, VT_FLOAT32}, // uv
            {2, 0, 4, VT_UINT8, True} // col
        };
    GraphicsPipeline.InputLayout.NumElements = _countof(VSInputs);
    GraphicsPipeline.InputLayout.LayoutElements = VSInputs;

    ShaderResourceVariableDesc Variables[] =
        {
            {SHADER_TYPE_PIXEL, "Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
        };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Variables;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Variables);

    SamplerDesc SamLinearWrap;
    SamLinearWrap.AddressU = TEXTURE_ADDRESS_WRAP;
    SamLinearWrap.AddressV = TEXTURE_ADDRESS_WRAP;
    SamLinearWrap.AddressW = TEXTURE_ADDRESS_WRAP;
    ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "Texture", SamLinearWrap} //
        };
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    {
        BufferDesc BuffDesc;
        BuffDesc.Size = sizeof(float4x4);
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pVertexConstantBuffer);
    }
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVertexConstantBuffer);

    CreateFontsTexture();
}

void ImGuiDiligentRenderer::CreateFontsTexture() {
    // Build texture atlas
    ImGuiIO &IO = ImGui::GetIO();

    unsigned char *pData = nullptr;
    int Width = 0;
    int Weight = 0;
    IO.Fonts->GetTexDataAsRGBA32(&pData, &Width, &Weight);

    TextureDesc FontTexDesc;
    FontTexDesc.Name = "Imgui font texture";
    FontTexDesc.Type = RESOURCE_DIM_TEX_2D;
    FontTexDesc.Width = static_cast<Uint32>(Width);
    FontTexDesc.Height = static_cast<Uint32>(Weight);
    FontTexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    FontTexDesc.BindFlags = BIND_SHADER_RESOURCE;
    FontTexDesc.Usage = USAGE_IMMUTABLE;

    TextureSubResData Mip0Data[] = {{pData, 4 * FontTexDesc.Width}};
    TextureData InitData(Mip0Data, _countof(Mip0Data));

    RefCntAutoPtr<ITexture> pFontTex;
    m_pDevice->CreateTexture(FontTexDesc, &InitData, &pFontTex);
    m_pFontSRV = pFontTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    m_pSRB.Release();
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    m_pTextureVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "Texture");
    VERIFY_EXPR(m_pTextureVar != nullptr);

    // Store our identifier
    IO.Fonts->TexID = (ImTextureID) m_pFontSRV;
}

float4 ImGuiDiligentRenderer::TransformClipRect(const ImVec2 &DisplaySize, const float4 &rect) const {
    switch (m_SurfacePreTransform) {
        case SURFACE_TRANSFORM_IDENTITY:
            return rect;

        case SURFACE_TRANSFORM_ROTATE_90: {
            // The image content is rotated 90 degrees clockwise. The origin is in the left-top corner.
            //
            //                                                             DsplSz.y
            //                a.x                                            -a.y     a.y     Old origin
            //              0---->|                                       0------->|<------| /
            //           0__|_____|____________________                0__|________|_______|/
            //            | |     '                    |                | |        '       |
            //        a.y | |     '                    |            a.x | |        '       |
            //           _V_|_ _ _a____b               |               _V_|_ _d'___a'      |
            //            A |     |    |               |                  |   |    |       |
            //  DsplSz.y  | |     |____|               |                  |   |____|       |
            //    -a.y    | |     d    c               |                  |   c'   b'      |
            //           _|_|__________________________|                  |                |
            //              A                                             |                |
            //              |-----> Y'                                    |                |
            //         New Origin                                         |________________|
            //
            float2 a{rect.x, rect.y};
            float2 c{rect.z, rect.w};
            return float4 //
                {
                    DisplaySize.y - c.y, // min_x = c'.x
                    a.x, // min_y = a'.y
                    DisplaySize.y - a.y, // max_x = a'.x
                    c.x // max_y = c'.y
                };
        }

        case SURFACE_TRANSFORM_ROTATE_180: {
            // The image content is rotated 180 degrees clockwise. The origin is in the left-top corner.
            //
            //                a.x                                               DsplSz.x - a.x
            //              0---->|                                         0------------------>|
            //           0__|_____|____________________                 0_ _|___________________|______
            //            | |     '                    |                  | |                   '      |
            //        a.y | |     '                    |        DsplSz.y  | |              c'___d'     |
            //           _V_|_ _ _a____b               |          -a.y    | |              |    |      |
            //              |     |    |               |                 _V_|_ _ _ _ _ _ _ |____|      |
            //              |     |____|               |                    |              b'   a'     |
            //              |     d    c               |                    |                          |
            //              |__________________________|                    |__________________________|
            //                                         A                                               A
            //                                         |                                               |
            //                                     New Origin                                      Old Origin
            float2 a{rect.x, rect.y};
            float2 c{rect.z, rect.w};
            return float4 //
                {
                    DisplaySize.x - c.x, // min_x = c'.x
                    DisplaySize.y - c.y, // min_y = c'.y
                    DisplaySize.x - a.x, // max_x = a'.x
                    DisplaySize.y - a.y // max_y = a'.y
                };
        }

        case SURFACE_TRANSFORM_ROTATE_270: {
            // The image content is rotated 270 degrees clockwise. The origin is in the left-top corner.
            //
            //              0  a.x     DsplSz.x-a.x   New Origin              a.y
            //              |---->|<-------------------|                    0----->|
            //          0_ _|_____|____________________V                 0 _|______|_________
            //            | |     '                    |                  | |      '         |
            //            | |     '                    |                  | |      '         |
            //        a.y_V_|_ _ _a____b               |        DsplSz.x  | |      '         |
            //              |     |    |               |          -a.x    | |      '         |
            //              |     |____|               |                  | |      b'___c'   |
            //              |     d    c               |                  | |      |    |    |
            //  DsplSz.y _ _|__________________________|                 _V_|_ _ _ |____|    |
            //                                                              |      a'   d'   |
            //                                                              |                |
            //                                                              |________________|
            //                                                              A
            //                                                              |
            //                                                            Old origin
            float2 a{rect.x, rect.y};
            float2 c{rect.z, rect.w};
            return float4 //
                {
                    a.y, // min_x = a'.x
                    DisplaySize.x - c.x, // min_y = c'.y
                    c.y, // max_x = c'.x
                    DisplaySize.x - a.x // max_y = a'.y
                };
        }

        case SURFACE_TRANSFORM_OPTIMAL:
                UNEXPECTED("SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
            return rect;

        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
                UNEXPECTED("Mirror transforms are not supported");
            return rect;

        default:
                UNEXPECTED("Unknown transform");
            return rect;
    }
}

static void BorschRenderDrawData(BorschDiligentViewportData *viewportData,
                                 ImDrawData *pDrawData) {

    auto Context = gTheApp->GetImmediateContext();

    ITextureView* pRTV = viewportData->pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = viewportData->pSwapChain->GetDepthBufferDSV();
    Context->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Avoid rendering when minimized
    if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
        return;

    // Create and grow vertex/index buffers if needed
    if (!viewportData->m_pVB || static_cast<int>(viewportData->m_VertexBufferSize) < pDrawData->TotalVtxCount) {
        viewportData->m_pVB.Release();
        while (static_cast<int>(viewportData->m_VertexBufferSize) < pDrawData->TotalVtxCount)
            viewportData->m_VertexBufferSize *= 2;

        BufferDesc VBDesc;
        VBDesc.Name = "Imgui vertex buffer";
        VBDesc.BindFlags = BIND_VERTEX_BUFFER;
        VBDesc.Size = viewportData->m_VertexBufferSize * sizeof(ImDrawVert);
        VBDesc.Usage = USAGE_DYNAMIC;
        VBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        RENDERER->m_pDevice->CreateBuffer(VBDesc, nullptr, &viewportData->m_pVB);
    }

    if (!viewportData->m_pIB || static_cast<int>(viewportData->m_IndexBufferSize) < pDrawData->TotalIdxCount) {
        viewportData->m_pIB.Release();
        while (static_cast<int>(viewportData->m_IndexBufferSize) < pDrawData->TotalIdxCount)
            viewportData->m_IndexBufferSize *= 2;

        BufferDesc IBDesc;
        IBDesc.Name = "Imgui index buffer";
        IBDesc.BindFlags = BIND_INDEX_BUFFER;
        IBDesc.Size = viewportData->m_IndexBufferSize * sizeof(ImDrawIdx);
        IBDesc.Usage = USAGE_DYNAMIC;
        IBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        RENDERER->m_pDevice->CreateBuffer(IBDesc, nullptr, &viewportData->m_pIB);
    }

    {
        MapHelper<ImDrawVert> Verices(Context, viewportData->m_pVB, MAP_WRITE, MAP_FLAG_DISCARD);
        MapHelper<ImDrawIdx> Indices(Context, viewportData->m_pIB, MAP_WRITE, MAP_FLAG_DISCARD);

        ImDrawVert *pVtxDst = Verices;
        ImDrawIdx *pIdxDst = Indices;
        for (Int32 CmdListID = 0; CmdListID < pDrawData->CmdListsCount; CmdListID++) {
            const ImDrawList *pCmdList = pDrawData->CmdLists[CmdListID];
            memcpy(pVtxDst, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(pIdxDst, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            pVtxDst += pCmdList->VtxBuffer.Size;
            pIdxDst += pCmdList->IdxBuffer.Size;
        }
    }

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from pDrawData->DisplayPos (top left) to pDrawData->DisplayPos+data_data->DisplaySize (bottom right).
    // DisplayPos is (0,0) for single viewport apps.
    {
        // DisplaySize always refers to the logical dimensions that account for pre-transform, hence
        // the aspect ratio will be correct after applying appropriate rotation.
        float L = pDrawData->DisplayPos.x;
        float R = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
        float T = pDrawData->DisplayPos.y;
        float B = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;

        // clang-format off
        float4x4 Projection
            {
                2.0f / (R - L), 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f / (T - B), 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f
            };
        // clang-format on

        // Bake pre-transform into projection
/*
        switch (m_SurfacePreTransform) {
            case SURFACE_TRANSFORM_IDENTITY:
                // Nothing to do
                break;

            case SURFACE_TRANSFORM_ROTATE_90:
                // The image content is rotated 90 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 0.5f);
                break;

            case SURFACE_TRANSFORM_ROTATE_180:
                // The image content is rotated 180 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 1.0f);
                break;

            case SURFACE_TRANSFORM_ROTATE_270:
                // The image content is rotated 270 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 1.5f);
                break;

            case SURFACE_TRANSFORM_OPTIMAL:
                    UNEXPECTED(
                    "SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
                break;

            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
                    UNEXPECTED("Mirror transforms are not supported");
                break;

            default:
                    UNEXPECTED("Unknown transform");
        }
*/

        MapHelper<float4x4> CBData(Context, RENDERER->m_pVertexConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBData = Projection;
    }

    auto SetupRenderState = [&]() //
    {
      // Setup shader and vertex buffers
      IBuffer *pVBs[] = {viewportData->m_pVB};
      Context->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);
      Context->SetIndexBuffer(viewportData->m_pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      Context->SetPipelineState(RENDERER->m_pPSO);

      const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
      Context->SetBlendFactors(blend_factor);

      Viewport vp;
      vp.Width = static_cast<float>(viewportData->Width) * pDrawData->FramebufferScale.x;
      vp.Height = static_cast<float>(viewportData->Height) * pDrawData->FramebufferScale.y;
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;
      vp.TopLeftX = vp.TopLeftY = 0;
      Context->SetViewports(1,
                            &vp,
                            static_cast<Uint32>(viewportData->Width * pDrawData->FramebufferScale.x),
                            static_cast<Uint32>(viewportData->Height * pDrawData->FramebufferScale.y));
    };

    SetupRenderState();

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    Uint32 GlobalIdxOffset = 0;
    Uint32 GlobalVtxOffset = 0;

    ITextureView *pLastTextureView = nullptr;
    for (Int32 CmdListID = 0; CmdListID < pDrawData->CmdListsCount; CmdListID++) {
        const ImDrawList *pCmdList = pDrawData->CmdLists[CmdListID];
        for (Int32 CmdID = 0; CmdID < pCmdList->CmdBuffer.Size; CmdID++) {
            const ImDrawCmd *pCmd = &pCmdList->CmdBuffer[CmdID];
            if (pCmd->UserCallback != NULL) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pCmd->UserCallback == ImDrawCallback_ResetRenderState)
                    SetupRenderState();
                else
                    pCmd->UserCallback(pCmdList, pCmd);
            } else {
                // Apply scissor/clipping rectangle
                float4 ClipRect //
                    {
                        (pCmd->ClipRect.x - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x,
                        (pCmd->ClipRect.y - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y,
                        (pCmd->ClipRect.z - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x,
                        (pCmd->ClipRect.w - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y //
                    };
                // Apply pretransform
                ClipRect = RENDERER->TransformClipRect(pDrawData->DisplaySize, ClipRect);

                Rect Scissor //
                    {
                        static_cast<Int32>(ClipRect.x),
                        static_cast<Int32>(ClipRect.y),
                        static_cast<Int32>(ClipRect.z),
                        static_cast<Int32>(ClipRect.w) //
                    };
                Context->SetScissorRects(1,
                                         &Scissor,
                                         static_cast<Uint32>(viewportData->Width * pDrawData->FramebufferScale.x),
                                         static_cast<Uint32>(viewportData->Height * pDrawData->FramebufferScale.y));

                // Bind texture
                auto *pTextureView = reinterpret_cast<ITextureView *>(pCmd->TextureId);
                VERIFY_EXPR(pTextureView);
                if (pTextureView != pLastTextureView) {
                    pLastTextureView = pTextureView;
                    RENDERER->m_pTextureVar->Set(pTextureView);
                    Context->CommitShaderResources(RENDERER->m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }

                DrawIndexedAttribs DrawAttrs{
                    pCmd->ElemCount, sizeof(ImDrawIdx) == sizeof(Uint16) ? VT_UINT16 : VT_UINT32,
                    DRAW_FLAG_VERIFY_STATES
                };
                DrawAttrs.FirstIndexLocation = pCmd->IdxOffset + GlobalIdxOffset;
                if (RENDERER->m_BaseVertexSupported) {
                    DrawAttrs.BaseVertex = pCmd->VtxOffset + GlobalVtxOffset;
                } else {
                    IBuffer *pVBs[] = {viewportData->m_pVB};
                    Uint64 VtxOffsets[] = {sizeof(ImDrawVert) * (pCmd->VtxOffset + GlobalVtxOffset)};
                    Context->SetVertexBuffers(0, 1, pVBs, VtxOffsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                              SET_VERTEX_BUFFERS_FLAG_NONE);
                }
                Context->DrawIndexed(DrawAttrs);
            }
        }
        GlobalIdxOffset += pCmdList->IdxBuffer.Size;
        GlobalVtxOffset += pCmdList->VtxBuffer.Size;
    }
}


void ImGuiDiligentRenderer::RenderDrawData(IDeviceContext *pCtx, ImDrawData *pDrawData) {
    // Avoid rendering when minimized
    if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
        return;

    // Create and grow vertex/index buffers if needed
    if (!m_pVB || static_cast<int>(m_VertexBufferSize) < pDrawData->TotalVtxCount) {
        m_pVB.Release();
        while (static_cast<int>(m_VertexBufferSize) < pDrawData->TotalVtxCount)
            m_VertexBufferSize *= 2;

        BufferDesc VBDesc;
        VBDesc.Name = "Imgui vertex buffer";
        VBDesc.BindFlags = BIND_VERTEX_BUFFER;
        VBDesc.Size = m_VertexBufferSize * sizeof(ImDrawVert);
        VBDesc.Usage = USAGE_DYNAMIC;
        VBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(VBDesc, nullptr, &m_pVB);
    }

    if (!m_pIB || static_cast<int>(m_IndexBufferSize) < pDrawData->TotalIdxCount) {
        m_pIB.Release();
        while (static_cast<int>(m_IndexBufferSize) < pDrawData->TotalIdxCount)
            m_IndexBufferSize *= 2;

        BufferDesc IBDesc;
        IBDesc.Name = "Imgui index buffer";
        IBDesc.BindFlags = BIND_INDEX_BUFFER;
        IBDesc.Size = m_IndexBufferSize * sizeof(ImDrawIdx);
        IBDesc.Usage = USAGE_DYNAMIC;
        IBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(IBDesc, nullptr, &m_pIB);
    }

    {
        MapHelper<ImDrawVert> Verices(pCtx, m_pVB, MAP_WRITE, MAP_FLAG_DISCARD);
        MapHelper<ImDrawIdx> Indices(pCtx, m_pIB, MAP_WRITE, MAP_FLAG_DISCARD);

        ImDrawVert *pVtxDst = Verices;
        ImDrawIdx *pIdxDst = Indices;
        for (Int32 CmdListID = 0; CmdListID < pDrawData->CmdListsCount; CmdListID++) {
            const ImDrawList *pCmdList = pDrawData->CmdLists[CmdListID];
            memcpy(pVtxDst, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(pIdxDst, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            pVtxDst += pCmdList->VtxBuffer.Size;
            pIdxDst += pCmdList->IdxBuffer.Size;
        }
    }

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from pDrawData->DisplayPos (top left) to pDrawData->DisplayPos+data_data->DisplaySize (bottom right).
    // DisplayPos is (0,0) for single viewport apps.
    {
        // DisplaySize always refers to the logical dimensions that account for pre-transform, hence
        // the aspect ratio will be correct after applying appropriate rotation.
        float L = pDrawData->DisplayPos.x;
        float R = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
        float T = pDrawData->DisplayPos.y;
        float B = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;

        // clang-format off
        float4x4 Projection
            {
                2.0f / (R - L), 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f / (T - B), 0.0f, 0.0f,
                0.0f, 0.0f, 0.5f, 0.0f,
                (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f
            };
        // clang-format on

        // Bake pre-transform into projection
        switch (m_SurfacePreTransform) {
            case SURFACE_TRANSFORM_IDENTITY:
                // Nothing to do
                break;

            case SURFACE_TRANSFORM_ROTATE_90:
                // The image content is rotated 90 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 0.5f);
                break;

            case SURFACE_TRANSFORM_ROTATE_180:
                // The image content is rotated 180 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 1.0f);
                break;

            case SURFACE_TRANSFORM_ROTATE_270:
                // The image content is rotated 270 degrees clockwise.
                Projection *= float4x4::RotationZ(-PI_F * 1.5f);
                break;

            case SURFACE_TRANSFORM_OPTIMAL:
                    UNEXPECTED(
                    "SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
                break;

            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
            case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
                    UNEXPECTED("Mirror transforms are not supported");
                break;

            default:
                    UNEXPECTED("Unknown transform");
        }

        MapHelper<float4x4> CBData(pCtx, m_pVertexConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBData = Projection;
    }

    auto SetupRenderState = [&]() //
    {
      // Setup shader and vertex buffers
      IBuffer *pVBs[] = {m_pVB};
      pCtx->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                             SET_VERTEX_BUFFERS_FLAG_RESET);
      pCtx->SetIndexBuffer(m_pIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      pCtx->SetPipelineState(m_pPSO);

      const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
      pCtx->SetBlendFactors(blend_factor);

      Viewport vp;
      vp.Width = static_cast<float>(m_RenderSurfaceWidth) * pDrawData->FramebufferScale.x;
      vp.Height = static_cast<float>(m_RenderSurfaceHeight) * pDrawData->FramebufferScale.y;
      vp.MinDepth = 0.0f;
      vp.MaxDepth = 1.0f;
      vp.TopLeftX = vp.TopLeftY = 0;
      pCtx->SetViewports(1,
                         &vp,
                         static_cast<Uint32>(m_RenderSurfaceWidth * pDrawData->FramebufferScale.x),
                         static_cast<Uint32>(m_RenderSurfaceHeight * pDrawData->FramebufferScale.y));
    };

    SetupRenderState();

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    Uint32 GlobalIdxOffset = 0;
    Uint32 GlobalVtxOffset = 0;

    ITextureView *pLastTextureView = nullptr;
    for (Int32 CmdListID = 0; CmdListID < pDrawData->CmdListsCount; CmdListID++) {
        const ImDrawList *pCmdList = pDrawData->CmdLists[CmdListID];
        for (Int32 CmdID = 0; CmdID < pCmdList->CmdBuffer.Size; CmdID++) {
            const ImDrawCmd *pCmd = &pCmdList->CmdBuffer[CmdID];
            if (pCmd->UserCallback != NULL) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pCmd->UserCallback == ImDrawCallback_ResetRenderState)
                    SetupRenderState();
                else
                    pCmd->UserCallback(pCmdList, pCmd);
            } else {
                // Apply scissor/clipping rectangle
                float4 ClipRect //
                    {
                        (pCmd->ClipRect.x - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x,
                        (pCmd->ClipRect.y - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y,
                        (pCmd->ClipRect.z - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x,
                        (pCmd->ClipRect.w - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y //
                    };
                // Apply pretransform
                ClipRect = TransformClipRect(pDrawData->DisplaySize, ClipRect);

                Rect Scissor //
                    {
                        static_cast<Int32>(ClipRect.x),
                        static_cast<Int32>(ClipRect.y),
                        static_cast<Int32>(ClipRect.z),
                        static_cast<Int32>(ClipRect.w) //
                    };
                pCtx->SetScissorRects(1,
                                      &Scissor,
                                      static_cast<Uint32>(m_RenderSurfaceWidth * pDrawData->FramebufferScale.x),
                                      static_cast<Uint32>(m_RenderSurfaceHeight * pDrawData->FramebufferScale.y));

                // Bind texture
                auto *pTextureView = reinterpret_cast<ITextureView *>(pCmd->TextureId);
                VERIFY_EXPR(pTextureView);
                if (pTextureView != pLastTextureView) {
                    pLastTextureView = pTextureView;
                    m_pTextureVar->Set(pTextureView);
                    pCtx->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                }

                DrawIndexedAttribs DrawAttrs{
                    pCmd->ElemCount, sizeof(ImDrawIdx) == sizeof(Uint16) ? VT_UINT16 : VT_UINT32,
                    DRAW_FLAG_VERIFY_STATES
                };
                DrawAttrs.FirstIndexLocation = pCmd->IdxOffset + GlobalIdxOffset;
                if (m_BaseVertexSupported) {
                    DrawAttrs.BaseVertex = pCmd->VtxOffset + GlobalVtxOffset;
                } else {
                    IBuffer *pVBs[] = {m_pVB};
                    Uint64 VtxOffsets[] = {sizeof(ImDrawVert) * (pCmd->VtxOffset + GlobalVtxOffset)};
                    pCtx->SetVertexBuffers(0, 1, pVBs, VtxOffsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                           SET_VERTEX_BUFFERS_FLAG_NONE);
                }
                pCtx->DrawIndexed(DrawAttrs);
            }
        }
        GlobalIdxOffset += pCmdList->IdxBuffer.Size;
        GlobalVtxOffset += pCmdList->VtxBuffer.Size;
    }
}
} // namespace Diligent

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplWin32_Data *ImGui_ImplWin32_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplWin32_Data *) ImGui::GetIO().BackendPlatformUserData : NULL;
}

static bool IsVkDown(int vk) {
    return (::GetKeyState(vk) & 0x8000) != 0;
}


static void ImGui_ImplWin32_UpdateKeyModifiers() {
    ImGuiIO &io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_ModCtrl, IsVkDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiKey_ModShift, IsVkDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiKey_ModAlt, IsVkDown(VK_MENU));
    io.AddKeyEvent(ImGuiKey_ModSuper, IsVkDown(VK_APPS));
}

#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

static void ImGui_ImplWin32_AddKeyEvent(ImGuiKey key, bool down, int native_keycode, int native_scancode = -1) {
    ImGuiIO &io = ImGui::GetIO();
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
    IM_UNUSED(native_scancode);
}

// Allow compilation with old Windows SDK. MinGW doesn't have default _WIN32_WINNT/WINVER versions.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef DBT_DEVNODES_CHANGED
#define DBT_DEVNODES_CHANGED 0x0007
#endif


// Map VK_xxx to ImGuiKey_xxx.
static ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam) {
    switch (wParam) {
        case VK_TAB:
            return ImGuiKey_Tab;
        case VK_LEFT:
            return ImGuiKey_LeftArrow;
        case VK_RIGHT:
            return ImGuiKey_RightArrow;
        case VK_UP:
            return ImGuiKey_UpArrow;
        case VK_DOWN:
            return ImGuiKey_DownArrow;
        case VK_PRIOR:
            return ImGuiKey_PageUp;
        case VK_NEXT:
            return ImGuiKey_PageDown;
        case VK_HOME:
            return ImGuiKey_Home;
        case VK_END:
            return ImGuiKey_End;
        case VK_INSERT:
            return ImGuiKey_Insert;
        case VK_DELETE:
            return ImGuiKey_Delete;
        case VK_BACK:
            return ImGuiKey_Backspace;
        case VK_SPACE:
            return ImGuiKey_Space;
        case VK_RETURN:
            return ImGuiKey_Enter;
        case VK_ESCAPE:
            return ImGuiKey_Escape;
        case VK_OEM_7:
            return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA:
            return ImGuiKey_Comma;
        case VK_OEM_MINUS:
            return ImGuiKey_Minus;
        case VK_OEM_PERIOD:
            return ImGuiKey_Period;
        case VK_OEM_2:
            return ImGuiKey_Slash;
        case VK_OEM_1:
            return ImGuiKey_Semicolon;
        case VK_OEM_PLUS:
            return ImGuiKey_Equal;
        case VK_OEM_4:
            return ImGuiKey_LeftBracket;
        case VK_OEM_5:
            return ImGuiKey_Backslash;
        case VK_OEM_6:
            return ImGuiKey_RightBracket;
        case VK_OEM_3:
            return ImGuiKey_GraveAccent;
        case VK_CAPITAL:
            return ImGuiKey_CapsLock;
        case VK_SCROLL:
            return ImGuiKey_ScrollLock;
        case VK_NUMLOCK:
            return ImGuiKey_NumLock;
        case VK_SNAPSHOT:
            return ImGuiKey_PrintScreen;
        case VK_PAUSE:
            return ImGuiKey_Pause;
        case VK_NUMPAD0:
            return ImGuiKey_Keypad0;
        case VK_NUMPAD1:
            return ImGuiKey_Keypad1;
        case VK_NUMPAD2:
            return ImGuiKey_Keypad2;
        case VK_NUMPAD3:
            return ImGuiKey_Keypad3;
        case VK_NUMPAD4:
            return ImGuiKey_Keypad4;
        case VK_NUMPAD5:
            return ImGuiKey_Keypad5;
        case VK_NUMPAD6:
            return ImGuiKey_Keypad6;
        case VK_NUMPAD7:
            return ImGuiKey_Keypad7;
        case VK_NUMPAD8:
            return ImGuiKey_Keypad8;
        case VK_NUMPAD9:
            return ImGuiKey_Keypad9;
        case VK_DECIMAL:
            return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT:
            return ImGuiKey_KeypadSubtract;
        case VK_ADD:
            return ImGuiKey_KeypadAdd;
        case IM_VK_KEYPAD_ENTER:
            return ImGuiKey_KeypadEnter;
        case VK_LSHIFT:
            return ImGuiKey_LeftShift;
        case VK_LCONTROL:
            return ImGuiKey_LeftCtrl;
        case VK_LMENU:
            return ImGuiKey_LeftAlt;
        case VK_LWIN:
            return ImGuiKey_LeftSuper;
        case VK_RSHIFT:
            return ImGuiKey_RightShift;
        case VK_RCONTROL:
            return ImGuiKey_RightCtrl;
        case VK_RMENU:
            return ImGuiKey_RightAlt;
        case VK_RWIN:
            return ImGuiKey_RightSuper;
        case VK_APPS:
            return ImGuiKey_Menu;
        case '0':
            return ImGuiKey_0;
        case '1':
            return ImGuiKey_1;
        case '2':
            return ImGuiKey_2;
        case '3':
            return ImGuiKey_3;
        case '4':
            return ImGuiKey_4;
        case '5':
            return ImGuiKey_5;
        case '6':
            return ImGuiKey_6;
        case '7':
            return ImGuiKey_7;
        case '8':
            return ImGuiKey_8;
        case '9':
            return ImGuiKey_9;
        case 'A':
            return ImGuiKey_A;
        case 'B':
            return ImGuiKey_B;
        case 'C':
            return ImGuiKey_C;
        case 'D':
            return ImGuiKey_D;
        case 'E':
            return ImGuiKey_E;
        case 'F':
            return ImGuiKey_F;
        case 'G':
            return ImGuiKey_G;
        case 'H':
            return ImGuiKey_H;
        case 'I':
            return ImGuiKey_I;
        case 'J':
            return ImGuiKey_J;
        case 'K':
            return ImGuiKey_K;
        case 'L':
            return ImGuiKey_L;
        case 'M':
            return ImGuiKey_M;
        case 'N':
            return ImGuiKey_N;
        case 'O':
            return ImGuiKey_O;
        case 'P':
            return ImGuiKey_P;
        case 'Q':
            return ImGuiKey_Q;
        case 'R':
            return ImGuiKey_R;
        case 'S':
            return ImGuiKey_S;
        case 'T':
            return ImGuiKey_T;
        case 'U':
            return ImGuiKey_U;
        case 'V':
            return ImGuiKey_V;
        case 'W':
            return ImGuiKey_W;
        case 'X':
            return ImGuiKey_X;
        case 'Y':
            return ImGuiKey_Y;
        case 'Z':
            return ImGuiKey_Z;
        case VK_F1:
            return ImGuiKey_F1;
        case VK_F2:
            return ImGuiKey_F2;
        case VK_F3:
            return ImGuiKey_F3;
        case VK_F4:
            return ImGuiKey_F4;
        case VK_F5:
            return ImGuiKey_F5;
        case VK_F6:
            return ImGuiKey_F6;
        case VK_F7:
            return ImGuiKey_F7;
        case VK_F8:
            return ImGuiKey_F8;
        case VK_F9:
            return ImGuiKey_F9;
        case VK_F10:
            return ImGuiKey_F10;
        case VK_F11:
            return ImGuiKey_F11;
        case VK_F12:
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
    }
}

static bool ImGui_ImplWin32_UpdateMouseCursor() {
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(NULL);
    } else {
        // Show OS mouse cursor
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor) {
            case ImGuiMouseCursor_Arrow:
                win32_cursor = IDC_ARROW;
                break;
            case ImGuiMouseCursor_TextInput:
                win32_cursor = IDC_IBEAM;
                break;
            case ImGuiMouseCursor_ResizeAll:
                win32_cursor = IDC_SIZEALL;
                break;
            case ImGuiMouseCursor_ResizeEW:
                win32_cursor = IDC_SIZEWE;
                break;
            case ImGuiMouseCursor_ResizeNS:
                win32_cursor = IDC_SIZENS;
                break;
            case ImGuiMouseCursor_ResizeNESW:
                win32_cursor = IDC_SIZENESW;
                break;
            case ImGuiMouseCursor_ResizeNWSE:
                win32_cursor = IDC_SIZENWSE;
                break;
            case ImGuiMouseCursor_Hand:
                win32_cursor = IDC_HAND;
                break;
            case ImGuiMouseCursor_NotAllowed:
                win32_cursor = IDC_NO;
                break;
        }
        ::SetCursor(::LoadCursor(NULL, win32_cursor));
    }
    return true;
}

LRESULT BORSCH_ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() == NULL)
        return 0;

    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplWin32_Data *bd = ImGui_ImplWin32_GetBackendData();

    switch (msg) {
        case WM_MOUSEMOVE: {
            // We need to call TrackMouseEvent in order to receive WM_MOUSELEAVE events
            bd->MouseHwnd = hwnd;
            if (!bd->MouseTracked) {
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
                ::TrackMouseEvent(&tme);
                bd->MouseTracked = true;
            }
            POINT mouse_pos = {(LONG) GET_X_LPARAM(lParam), (LONG) GET_Y_LPARAM(lParam)};
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                ::ClientToScreen(hwnd, &mouse_pos);
            io.AddMousePosEvent((float) mouse_pos.x, (float) mouse_pos.y);
            break;
        }
        case WM_MOUSELEAVE:
            if (bd->MouseHwnd == hwnd)
                bd->MouseHwnd = NULL;
            bd->MouseTracked = false;
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: {
            int button = 0;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
            if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
            if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
            if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) {
                button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;
            }
            if (bd->MouseButtonsDown == 0 && ::GetCapture() == NULL)
                ::SetCapture(hwnd);
            bd->MouseButtonsDown |= 1 << button;
            io.AddMouseButtonEvent(button, true);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP: {
            int button = 0;
            if (msg == WM_LBUTTONUP) { button = 0; }
            if (msg == WM_RBUTTONUP) { button = 1; }
            if (msg == WM_MBUTTONUP) { button = 2; }
            if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
            bd->MouseButtonsDown &= ~(1 << button);
            if (bd->MouseButtonsDown == 0 && ::GetCapture() == hwnd)
                ::ReleaseCapture();
            io.AddMouseButtonEvent(button, false);
            return 0;
        }
        case WM_MOUSEWHEEL:
            io.AddMouseWheelEvent(0.0f, (float) GET_WHEEL_DELTA_WPARAM(wParam) / (float) WHEEL_DELTA);
            return 0;
        case WM_MOUSEHWHEEL:
            io.AddMouseWheelEvent((float) GET_WHEEL_DELTA_WPARAM(wParam) / (float) WHEEL_DELTA, 0.0f);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            const bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            if (wParam < 256) {
                // Submit modifiers
                ImGui_ImplWin32_UpdateKeyModifiers();

                // Obtain virtual key code
                // (keypad enter doesn't have its own... VK_RETURN with KF_EXTENDED flag means keypad enter, see IM_VK_KEYPAD_ENTER definition for details, it is mapped to ImGuiKey_KeyPadEnter.)
                int vk = (int) wParam;
                if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
                    vk = IM_VK_KEYPAD_ENTER;

                // Submit key event
                const ImGuiKey key = ImGui_ImplWin32_VirtualKeyToImGuiKey(vk);
                const int scancode = (int) LOBYTE(HIWORD(lParam));
                if (key != ImGuiKey_None)
                    ImGui_ImplWin32_AddKeyEvent(key, is_key_down, vk, scancode);

                // Submit individual left/right modifier events
                if (vk == VK_SHIFT) {
                    // Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
                    if (IsVkDown(VK_LSHIFT) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftShift, is_key_down, VK_LSHIFT, scancode);
                    }
                    if (IsVkDown(VK_RSHIFT) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightShift, is_key_down, VK_RSHIFT, scancode);
                    }
                } else if (vk == VK_CONTROL) {
                    if (IsVkDown(VK_LCONTROL) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftCtrl, is_key_down, VK_LCONTROL, scancode);
                    }
                    if (IsVkDown(VK_RCONTROL) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightCtrl, is_key_down, VK_RCONTROL, scancode);
                    }
                } else if (vk == VK_MENU) {
                    if (IsVkDown(VK_LMENU) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_LeftAlt, is_key_down, VK_LMENU, scancode);
                    }
                    if (IsVkDown(VK_RMENU) == is_key_down) {
                        ImGui_ImplWin32_AddKeyEvent(ImGuiKey_RightAlt, is_key_down, VK_RMENU, scancode);
                    }
                }
            }
            return 0;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            io.AddFocusEvent(msg == WM_SETFOCUS);
            return 0;
        case WM_CHAR:
            // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((unsigned short) wParam);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && ImGui_ImplWin32_UpdateMouseCursor())
                return 1;
            return 0;
        case WM_DEVICECHANGE:
            if ((UINT) wParam == DBT_DEVNODES_CHANGED)
                bd->WantUpdateHasGamepad = true;
            return 0;
        case WM_DISPLAYCHANGE:
            bd->WantUpdateMonitors = true;
            return 0;
    }
    return 0;
}


static LRESULT
CALLBACK ImGui_ImplWin32_WndProcHandler_PlatformWindow(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (BORSCH_ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (ImGuiViewport *viewport = ImGui::FindViewportByPlatformHandle((void *) hWnd)) {
        switch (msg) {
            case WM_CLOSE:
                viewport->PlatformRequestClose = true;
                return 0;
            case WM_MOVE:
                viewport->PlatformRequestMove = true;
                break;
            case WM_SIZE:
                viewport->PlatformRequestResize = true;
                break;
            case WM_MOUSEACTIVATE:
                if (viewport->Flags & ImGuiViewportFlags_NoFocusOnClick)
                    return MA_NOACTIVATE;
                break;
            case WM_NCHITTEST:
                // Let mouse pass-through the window. This will allow the backend to call io.AddMouseViewportEvent() correctly. (which is optional).
                // The ImGuiViewportFlags_NoInputs flag is set while dragging a viewport, as want to detect the window behind the one we are dragging.
                // If you cannot easily access those viewport flags from your windowing/event code: you may manually synchronize its state e.g. in
                // your main loop after calling UpdatePlatformWindows(). Iterate all viewports/platform windows and pass the flag to your windowing system.
                if (viewport->Flags & ImGuiViewportFlags_NoInputs)
                    return HTTRANSPARENT;
                break;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

float ImGui_ImplWin32_GetDpiScaleForMonitor(void *monitor) {
    UINT xdpi = 96, ydpi = 96;
    if (_IsWindows8Point1OrGreater()) {
        static HINSTANCE shcore_dll = ::LoadLibraryA("shcore.dll"); // Reference counted per-process
        static PFN_GetDpiForMonitor GetDpiForMonitorFn = NULL;
        if (GetDpiForMonitorFn == NULL && shcore_dll != NULL)
            GetDpiForMonitorFn = (PFN_GetDpiForMonitor) ::GetProcAddress(shcore_dll, "GetDpiForMonitor");
        if (GetDpiForMonitorFn != NULL) {
            GetDpiForMonitorFn((HMONITOR) monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
            IM_ASSERT(xdpi == ydpi); // Please contact me if you hit this assert!
            return xdpi / 96.0f;
        }
    }
#ifndef NOGDI
    const HDC dc = ::GetDC(NULL);
    xdpi = ::GetDeviceCaps(dc, LOGPIXELSX);
    ydpi = ::GetDeviceCaps(dc, LOGPIXELSY);
    IM_ASSERT(xdpi == ydpi); // Please contact me if you hit this assert!
    ::ReleaseDC(NULL, dc);
#endif
    return xdpi / 96.0f;
}

static BOOL CALLBACK ImGui_ImplWin32_UpdateMonitors_EnumFunc(HMONITOR monitor, HDC, LPRECT, LPARAM) {
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    if (!::GetMonitorInfo(monitor, &info))
        return TRUE;
    ImGuiPlatformMonitor imgui_monitor;
    imgui_monitor.MainPos = ImVec2((float) info.rcMonitor.left, (float) info.rcMonitor.top);
    imgui_monitor.MainSize = ImVec2((float) (info.rcMonitor.right - info.rcMonitor.left),
                                    (float) (info.rcMonitor.bottom - info.rcMonitor.top));
    imgui_monitor.WorkPos = ImVec2((float) info.rcWork.left, (float) info.rcWork.top);
    imgui_monitor.WorkSize = ImVec2((float) (info.rcWork.right - info.rcWork.left),
                                    (float) (info.rcWork.bottom - info.rcWork.top));
    imgui_monitor.DpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    ImGuiPlatformIO &io = ImGui::GetPlatformIO();
    if (info.dwFlags & MONITORINFOF_PRIMARY)
        io.Monitors.push_front(imgui_monitor);
    else
        io.Monitors.push_back(imgui_monitor);
    return TRUE;
}

static void ImGui_ImplWin32_UpdateMonitors() {
    ImGui_ImplWin32_Data *bd = ImGui_ImplWin32_GetBackendData();
    ImGui::GetPlatformIO().Monitors.resize(0);
    ::EnumDisplayMonitors(NULL, NULL, ImGui_ImplWin32_UpdateMonitors_EnumFunc, 0);
    bd->WantUpdateMonitors = false;
}

static void
ImGui_ImplWin32_GetWin32StyleFromViewportFlags(ImGuiViewportFlags flags, DWORD *out_style, DWORD *out_ex_style) {
    if (flags & ImGuiViewportFlags_NoDecoration)
        *out_style = WS_POPUP;
    else
        *out_style = WS_OVERLAPPEDWINDOW;

    if (flags & ImGuiViewportFlags_NoTaskBarIcon)
        *out_ex_style = WS_EX_TOOLWINDOW;
    else
        *out_ex_style = WS_EX_APPWINDOW;

    if (flags & ImGuiViewportFlags_TopMost)
        *out_ex_style |= WS_EX_TOPMOST;
}

static void ImGui_ImplWin32_CreateWindow(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    viewport->PlatformUserData = vd;

    // Select style and parent window
    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &vd->DwStyle, &vd->DwExStyle);
    HWND parent_window = NULL;
    if (viewport->ParentViewportId != 0)
        if (ImGuiViewport *parent_viewport = ImGui::FindViewportByID(viewport->ParentViewportId))
            parent_window = (HWND) parent_viewport->PlatformHandle;

    // Create window
    RECT rect = {(LONG) viewport->Pos.x, (LONG) viewport->Pos.y, (LONG) (viewport->Pos.x + viewport->Size.x),
                 (LONG) (viewport->Pos.y + viewport->Size.y)};
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle);
    vd->Hwnd = ::CreateWindowEx(
        vd->DwExStyle, _T("ImGui Platform"), _T("Untitled"), vd->DwStyle,   // Style, class name, window name
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,    // Window area
        parent_window, NULL, ::GetModuleHandle(NULL), NULL);                    // Parent window, Menu, Instance, Param
    vd->HwndOwned = true;
    viewport->PlatformRequestResize = false;
    viewport->PlatformHandle = viewport->PlatformHandleRaw = vd->Hwnd;
}

static void ImGui_ImplWin32_DestroyWindow(ImGuiViewport *viewport) {
    ImGui_ImplWin32_Data *bd = ImGui_ImplWin32_GetBackendData();
    if (ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData) {
        if (::GetCapture() == vd->Hwnd) {
            // Transfer capture so if we started dragging from a window that later disappears, we'll still receive the MOUSEUP event.
            ::ReleaseCapture();
            ::SetCapture(bd->hWnd);
        }
        if (vd->Hwnd && vd->HwndOwned)
            ::DestroyWindow(vd->Hwnd);
        vd->Hwnd = NULL;
        IM_DELETE(vd);
    }
    viewport->PlatformUserData = viewport->PlatformHandle = NULL;
}

static void ImGui_ImplWin32_ShowWindow(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
        ::ShowWindow(vd->Hwnd, SW_SHOWNA);
    else
        ::ShowWindow(vd->Hwnd, SW_SHOW);
}

static ImVec2 ImGui_ImplWin32_GetWindowPos(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    POINT pos = {0, 0};
    ::ClientToScreen(vd->Hwnd, &pos);
    return ImVec2((float) pos.x, (float) pos.y);
}

static void ImGui_ImplWin32_SetWindowPos(ImGuiViewport *viewport, ImVec2 pos) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = {(LONG) pos.x, (LONG) pos.y, (LONG) pos.x, (LONG) pos.y};
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle);
    ::SetWindowPos(vd->Hwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static ImVec2 ImGui_ImplWin32_GetWindowSize(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect;
    ::GetClientRect(vd->Hwnd, &rect);
    return ImVec2(float(rect.right - rect.left), float(rect.bottom - rect.top));
}

static void ImGui_ImplWin32_SetWindowSize(ImGuiViewport *viewport, ImVec2 size) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    RECT rect = {0, 0, (LONG) size.x, (LONG) size.y};
    ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle); // Client to Screen
    ::SetWindowPos(vd->Hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                   SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

static void ImGui_ImplWin32_SetWindowFocus(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    ::BringWindowToTop(vd->Hwnd);
    ::SetForegroundWindow(vd->Hwnd);
    ::SetFocus(vd->Hwnd);
}

static bool ImGui_ImplWin32_GetWindowFocus(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::GetForegroundWindow() == vd->Hwnd;
}

static bool ImGui_ImplWin32_GetWindowMinimized(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ::IsIconic(vd->Hwnd) != 0;
}

static void ImGui_ImplWin32_SetWindowTitle(ImGuiViewport *viewport, const char *title) {
    // ::SetWindowTextA() doesn't properly handle UTF-8 so we explicitely convert our string.
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    int n = ::MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    ImVector<wchar_t> title_w;
    title_w.resize(n);
    ::MultiByteToWideChar(CP_UTF8, 0, title, -1, title_w.Data, n);
    ::SetWindowTextW(vd->Hwnd, title_w.Data);
}

static void ImGui_ImplWin32_SetWindowAlpha(ImGuiViewport *viewport, float alpha) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    IM_ASSERT(alpha >= 0.0f && alpha <= 1.0f);
    if (alpha < 1.0f) {
        DWORD style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) | WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, style);
        ::SetLayeredWindowAttributes(vd->Hwnd, 0, (BYTE) (255 * alpha), LWA_ALPHA);
    } else {
        DWORD style = ::GetWindowLongW(vd->Hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED;
        ::SetWindowLongW(vd->Hwnd, GWL_EXSTYLE, style);
    }
}

float ImGui_ImplWin32_GetDpiScaleForHwnd(void *hwnd) {
    HMONITOR monitor = ::MonitorFromWindow((HWND) hwnd, MONITOR_DEFAULTTONEAREST);
    return ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
}

static float ImGui_ImplWin32_GetWindowDpiScale(ImGuiViewport *viewport) {
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    return ImGui_ImplWin32_GetDpiScaleForHwnd(vd->Hwnd);
}

// FIXME-DPI: Testing DPI related ideas
static void ImGui_ImplWin32_OnChangedViewport(ImGuiViewport *viewport) {
    (void) viewport;
#if 0
    ImGuiStyle default_style;
    //default_style.WindowPadding = ImVec2(0, 0);
    //default_style.WindowBorderSize = 0.0f;
    //default_style.ItemSpacing.y = 3.0f;
    //default_style.FramePadding = ImVec2(0, 0);
    default_style.ScaleAllSizes(viewport->DpiScale);
    ImGuiStyle& style = ImGui::GetStyle();
    style = default_style;
#endif
}

static void ImGui_ImplWin32_UpdateWindow(ImGuiViewport *viewport) {
    // (Optional) Update Win32 style if it changed _after_ creation.
    // Generally they won't change unless configuration flags are changed, but advanced uses (such as manually rewriting viewport flags) make this useful.
    ImGui_ImplWin32_ViewportData *vd = (ImGui_ImplWin32_ViewportData *) viewport->PlatformUserData;
    IM_ASSERT(vd->Hwnd != 0);
    DWORD new_style;
    DWORD new_ex_style;
    ImGui_ImplWin32_GetWin32StyleFromViewportFlags(viewport->Flags, &new_style, &new_ex_style);

    // Only reapply the flags that have been changed from our point of view (as other flags are being modified by Windows)
    if (vd->DwStyle != new_style || vd->DwExStyle != new_ex_style) {
        // (Optional) Update TopMost state if it changed _after_ creation
        bool top_most_changed = (vd->DwExStyle & WS_EX_TOPMOST) != (new_ex_style & WS_EX_TOPMOST);
        HWND insert_after = top_most_changed ? ((viewport->Flags & ImGuiViewportFlags_TopMost) ? HWND_TOPMOST
                                                                                               : HWND_NOTOPMOST) : 0;
        UINT swp_flag = top_most_changed ? 0 : SWP_NOZORDER;

        // Apply flags and position (since it is affected by flags)
        vd->DwStyle = new_style;
        vd->DwExStyle = new_ex_style;
        ::SetWindowLong(vd->Hwnd, GWL_STYLE, vd->DwStyle);
        ::SetWindowLong(vd->Hwnd, GWL_EXSTYLE, vd->DwExStyle);
        RECT rect = {(LONG) viewport->Pos.x, (LONG) viewport->Pos.y, (LONG) (viewport->Pos.x + viewport->Size.x),
                     (LONG) (viewport->Pos.y + viewport->Size.y)};
        ::AdjustWindowRectEx(&rect, vd->DwStyle, FALSE, vd->DwExStyle); // Client to Screen
        ::SetWindowPos(vd->Hwnd, insert_after, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                       swp_flag | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ::ShowWindow(vd->Hwnd, SW_SHOWNA); // This is necessary when we alter the style
        viewport->PlatformRequestMove = viewport->PlatformRequestResize = true;
    }
}


static void ImGui_ImplWin32_InitPlatformInterface() {
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = ImGui_ImplWin32_WndProcHandler_PlatformWindow;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = ::GetModuleHandle(NULL);
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = (HBRUSH) (COLOR_BACKGROUND + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _T("ImGui Platform");
    wcex.hIconSm = NULL;
    ::RegisterClassEx(&wcex);

    ImGui_ImplWin32_UpdateMonitors();

    // Register platform interface (will be coupled with a renderer interface)
    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateWindow = ImGui_ImplWin32_CreateWindow;
    platform_io.Platform_DestroyWindow = ImGui_ImplWin32_DestroyWindow;
    platform_io.Platform_ShowWindow = ImGui_ImplWin32_ShowWindow;
    platform_io.Platform_SetWindowPos = ImGui_ImplWin32_SetWindowPos;
    platform_io.Platform_GetWindowPos = ImGui_ImplWin32_GetWindowPos;
    platform_io.Platform_SetWindowSize = ImGui_ImplWin32_SetWindowSize;
    platform_io.Platform_GetWindowSize = ImGui_ImplWin32_GetWindowSize;
    platform_io.Platform_SetWindowFocus = ImGui_ImplWin32_SetWindowFocus;
    platform_io.Platform_GetWindowFocus = ImGui_ImplWin32_GetWindowFocus;
    platform_io.Platform_GetWindowMinimized = ImGui_ImplWin32_GetWindowMinimized;
    platform_io.Platform_SetWindowTitle = ImGui_ImplWin32_SetWindowTitle;
    platform_io.Platform_SetWindowAlpha = ImGui_ImplWin32_SetWindowAlpha;
    platform_io.Platform_UpdateWindow = ImGui_ImplWin32_UpdateWindow;
    platform_io.Platform_GetWindowDpiScale = ImGui_ImplWin32_GetWindowDpiScale; // FIXME-DPI
    platform_io.Platform_OnChangedViewport = ImGui_ImplWin32_OnChangedViewport; // FIXME-DPI

    // Register main window handle (which is owned by the main application, not by us)
    // This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
    ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    ImGui_ImplWin32_Data *bd = ImGui_ImplWin32_GetBackendData();
    ImGui_ImplWin32_ViewportData *vd = IM_NEW(ImGui_ImplWin32_ViewportData)();
    vd->Hwnd = bd->hWnd;
    vd->HwndOwned = false;
    main_viewport->PlatformUserData = vd;
    main_viewport->PlatformHandle = (void *) bd->hWnd;
}

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static BorschDiligentRenderData *ImGui_ImplDX12_GetBackendData() {
    return ImGui::GetCurrentContext() ? (BorschDiligentRenderData *) ImGui::GetIO().BackendRendererUserData : NULL;
}

static void Diligent_CreateWindow(ImGuiViewport *viewport) {
    HWND hwnd = viewport->PlatformHandleRaw ? (HWND) viewport->PlatformHandleRaw : (HWND) viewport->PlatformHandle;

    IM_ASSERT(hwnd != 0);

    BorschDiligentRenderData *bd = ImGui_ImplDX12_GetBackendData();
    BorschDiligentViewportData *vd = IM_NEW(BorschDiligentViewportData)(/*bd->numFramesInFlight*/);
    viewport->RendererUserData = vd;

    bt::gTheApp->CreateSwapChain(vd->pSwapChain, hwnd, true);

    IM_ASSERT(vd->pSwapChain.RawPtr() != nullptr);
/*
    vd->FrameIndex = UINT_MAX;

    // Create command queue.
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT res = S_OK;
    res = bd->pd3dDevice->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&vd->CommandQueue));
    IM_ASSERT(res == S_OK);

    // Create command allocator.
    for (UINT i = 0; i < bd->numFramesInFlight; ++i)
    {
        res = bd->pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&vd->FrameCtx[i].CommandAllocator));
        IM_ASSERT(res == S_OK);
    }

    // Create command list.
    res = bd->pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, vd->FrameCtx[0].CommandAllocator, NULL, IID_PPV_ARGS(&vd->CommandList));
    IM_ASSERT(res == S_OK);
    vd->CommandList->Close();

    // Create fence.
    res = bd->pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&vd->Fence));
    IM_ASSERT(res == S_OK);

    vd->FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    IM_ASSERT(vd->FenceEvent != NULL);

    // Create swap chain
    // FIXME-VIEWPORT: May want to copy/inherit swap chain settings from the user/application.
    DXGI_SWAP_CHAIN_DESC1 sd1;
    ZeroMemory(&sd1, sizeof(sd1));
    sd1.BufferCount = bd->numFramesInFlight;
    sd1.Width = (UINT)viewport->Size.x;
    sd1.Height = (UINT)viewport->Size.y;
    sd1.Format = bd->RTVFormat;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.SampleDesc.Count = 1;
    sd1.SampleDesc.Quality = 0;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    sd1.Scaling = DXGI_SCALING_STRETCH;
    sd1.Stereo = FALSE;

    IDXGIFactory4* dxgi_factory = NULL;
    res = ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    IM_ASSERT(res == S_OK);

    IDXGISwapChain1* swap_chain = NULL;
    res = dxgi_factory->CreateSwapChainForHwnd(vd->CommandQueue, hwnd, &sd1, NULL, NULL, &swap_chain);
    IM_ASSERT(res == S_OK);

    dxgi_factory->Release();

    // Or swapChain.As(&mSwapChain)
    IM_ASSERT(vd->SwapChain == NULL);
    swap_chain->QueryInterface(IID_PPV_ARGS(&vd->SwapChain));
    swap_chain->Release();

    // Create the render targets
    if (vd->SwapChain)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = bd->numFramesInFlight;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;

        HRESULT hr = bd->pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&vd->RtvDescHeap));
        IM_ASSERT(hr == S_OK);

        SIZE_T rtv_descriptor_size = bd->pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = vd->RtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < bd->numFramesInFlight; i++)
        {
            vd->FrameCtx[i].RenderTargetCpuDescriptors = rtv_handle;
            rtv_handle.ptr += rtv_descriptor_size;
        }

        ID3D12Resource* back_buffer;
        for (UINT i = 0; i < bd->numFramesInFlight; i++)
        {
            IM_ASSERT(vd->FrameCtx[i].RenderTarget == NULL);
            vd->SwapChain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
            bd->pd3dDevice->CreateRenderTargetView(back_buffer, NULL, vd->FrameCtx[i].RenderTargetCpuDescriptors);
            vd->FrameCtx[i].RenderTarget = back_buffer;
        }
    }

    for (UINT i = 0; i < bd->numFramesInFlight; i++)
        ImGui_ImplDX12_DestroyRenderBuffers(&vd->FrameRenderBuffers[i]);
*/
}

static void ImGui_ImplDX12_DestroyWindow(ImGuiViewport *viewport) {
    // The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
    BorschDiligentRenderData *bd = ImGui_ImplDX12_GetBackendData();
    if (BorschDiligentViewportData *vd = (BorschDiligentViewportData *) viewport->RendererUserData) {
        //ImGui_WaitForPendingOperations(vd);

        /*  SafeRelease(vd->CommandQueue);
          SafeRelease(vd->CommandList);
          SafeRelease(vd->SwapChain);
          SafeRelease(vd->RtvDescHeap);
          SafeRelease(vd->Fence);
          ::CloseHandle(vd->FenceEvent);
          vd->FenceEvent = NULL;

          for (UINT i = 0; i < bd->numFramesInFlight; i++)
          {
              SafeRelease(vd->FrameCtx[i].RenderTarget);
              SafeRelease(vd->FrameCtx[i].CommandAllocator);
              ImGui_ImplDX12_DestroyRenderBuffers(&vd->FrameRenderBuffers[i]);
          }
          IM_DELETE(vd);*/
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_ImplDX12_SetWindowSize(ImGuiViewport *viewport, ImVec2 size) {
    BorschDiligentRenderData *bd = ImGui_ImplDX12_GetBackendData();
    BorschDiligentViewportData *vd = (BorschDiligentViewportData *) viewport->RendererUserData;

    //ImGui_WaitForPendingOperations(vd);

    // for (UINT i = 0; i < bd->numFramesInFlight; i++)
    //     SafeRelease(vd->FrameCtx[i].RenderTarget);

    if (vd->pSwapChain) {
        vd->pSwapChain->Resize(size.x, size.y);
        /* ID3D12Resource* back_buffer = NULL;
         vd->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
         for (UINT i = 0; i < bd->numFramesInFlight; i++)
         {
             vd->SwapChain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
             bd->pd3dDevice->CreateRenderTargetView(back_buffer, NULL, vd->FrameCtx[i].RenderTargetCpuDescriptors);
             vd->FrameCtx[i].RenderTarget = back_buffer;
         }*/
    }
}

static void ImGui_ImplDX12_RenderWindow(ImGuiViewport *viewport, void *) {
    bt::log::Debug("RenderWindow" + std::to_string(viewport->Size.x));

    //ImGui_ImplDX12_Data* bd = ImGui_ImplDX12_GetBackendData();
    BorschDiligentViewportData *vd = (BorschDiligentViewportData *) viewport->RendererUserData;

    vd->Width = viewport->Size.x;
    vd->Height = viewport->Size.y;
    bt::BorschRenderDrawData(vd, viewport->DrawData);
/*
    ImGui_ImplDX12_FrameContext* frame_context = &vd->FrameCtx[vd->FrameIndex % bd->numFramesInFlight];
    UINT back_buffer_idx = vd->SwapChain->GetCurrentBackBufferIndex();

    const ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = vd->FrameCtx[back_buffer_idx].RenderTarget;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Draw
    ID3D12GraphicsCommandList* cmd_list = vd->CommandList;

    frame_context->CommandAllocator->Reset();
    cmd_list->Reset(frame_context->CommandAllocator, NULL);
    cmd_list->ResourceBarrier(1, &barrier);
    cmd_list->OMSetRenderTargets(1, &vd->FrameCtx[back_buffer_idx].RenderTargetCpuDescriptors, FALSE, NULL);
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
        cmd_list->ClearRenderTargetView(vd->FrameCtx[back_buffer_idx].RenderTargetCpuDescriptors, (float*)&clear_color, 0, NULL);
    cmd_list->SetDescriptorHeaps(1, &bd->pd3dSrvDescHeap);

    ImGui_ImplDX12_RenderDrawData(viewport->DrawData, cmd_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmd_list->ResourceBarrier(1, &barrier);
    cmd_list->Close();

    vd->CommandQueue->Wait(vd->Fence, vd->FenceSignaledValue);
    vd->CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmd_list);
    vd->CommandQueue->Signal(vd->Fence, ++vd->FenceSignaledValue);
*/
}

static void ImGui_ImplDX12_SwapBuffers(ImGuiViewport *viewport, void *) {
    auto *vd = (BorschDiligentViewportData *) viewport->RendererUserData;

    vd->pSwapChain->Present();
    /* while (vd->Fence->GetCompletedValue() < vd->FenceSignaledValue)
         ::SwitchToThread();*/
}

static void ImGui_ImplDX12_InitPlatformInterface() {
    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = Diligent_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplDX12_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplDX12_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplDX12_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_ImplDX12_SwapBuffers;
}
