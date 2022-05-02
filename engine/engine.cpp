
#include <stdio.h>
#include <bx/uint32_t.h>
#include <bgfx/bgfx.h>

#if BX_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#elif BX_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#elif BX_PLATFORM_OSX
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

static bool s_showStats = false;

static void glfw_errorCallback(int error, const char *description)
{
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void glfw_keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F1 && action == GLFW_RELEASE)
        s_showStats = !s_showStats;
}

int main(int argc, char* argv[])
{
    bgfx::Init init;
    //init.type     = args.m_type;
    //init.vendorId = args.m_pciId;
    init.resolution.width  = 800;
    init.resolution.height = 600;
    init.resolution.reset  = BGFX_RESET_VSYNC;
    bgfx::init(init);
    // Enable debug text.
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    // Set view 0 clear state.
    bgfx::setViewClear(0
        , BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
        , 0x303030ff
        , 1.0f
        , 0
        );
    bgfx::frame();

    WindowHandle defaultWindow = { 0 };
    setWindowSize(defaultWindow, s_width, s_height);

#if BX_PLATFORM_EMSCRIPTEN
    s_app = _app;
    emscripten_set_main_loop(&updateApp, -1, 1);
#else
    while (_app->update() )
    {
        if (0 != bx::strLen(s_restartArgs) )
        {
            break;
        }
    }
#endif // BX_PLATFORM_EMSCRIPTEN

    return _app->shutdown();
    
    return 0;
}
