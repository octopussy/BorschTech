cmake_minimum_required(VERSION 3.16)
project(engine VERSION 0.1.0)

# find_package(Vulkan REQUIRED FATAL_ERROR) # error

set(CMAKE_CONFIGURATION_TYPES "Release" "Debug")

set(THIRD_PARTY_DIR 3rdparty)

if (MSVC)
    if (USE_MSVC_RUNTIME_LIBRARY_DLL)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()


if(MSVC)
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
endif()

add_definitions(-DSFML_STATIC)
add_definitions(-DMINFFT_SINGLE)

# Diligent
add_subdirectory(${THIRD_PARTY_DIR}/DiligentCore)
add_subdirectory(${THIRD_PARTY_DIR}/DiligentTools)
include_directories(${THIRD_PARTY_DIR}/DiligentCore)
include_directories(${THIRD_PARTY_DIR}/DiligentTools)
#include_directories(DiligentCore/Common/Interface)

# daScript 
set(DAS_BUILD_TUTORIAL NO)
set(DAS_BUILD_TEST NO)
set(DAS_BUILD_PROFILE NO)
set(DAS_GLFW_DISABLED YES)
add_subdirectory(${THIRD_PARTY_DIR}/daScript)
include_directories(${THIRD_PARTY_DIR}/daScript/include)

# GLFW
set(GLFW_DIR ${THIRD_PARTY_DIR}/glfw) # Set this to point to an up-to-date GLFW repo
set(GLFW_LIBRARIES ${GLFW_DIR}/glfw/lib/glfw3.lib)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
# add_subdirectory(${GLFW_DIR} binary_dir EXCLUDE_FROM_ALL)
# add_subdirectory(${GLFW_DIR})
# include_directories(${GLFW_DIR}/include Vulkan::Headers)

# GLM
add_subdirectory(${THIRD_PARTY_DIR}/glm)
# set(GLM_DIR ${THIRD_PARTY_DIR}/glm)
# include_directories(${GLM_DIR})

# imgui
set(IMGUI_DIR ${THIRD_PARTY_DIR}/imgui)

include(CTest)
enable_testing()

add_executable(engine WIN32
        engine/src/Engine.cpp

     #   ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
     #   ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
     #   ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
     #   ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp

        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        

        #engine/src/main.cpp
        engine/src/ImGuiDiligentRenderer.cpp
        engine/src/ImGuiDiligentRenderer.hpp
        engine/src/ImGuiImpl.cpp
        engine/src/ImGuiImpl.hpp
        engine/src/ImGuiImplWin32.cpp
        engine/src/Application.cpp
        engine/src/Camera.cpp
        engine/src/Input/InputManager.cpp
        engine/src/core/Logging.cpp engine/src/Scene.h engine/src/Scene.cpp engine/src/editor/Editor.h engine/src/editor/Editor.cpp engine/src/editor/TestCube.h engine/src/editor/TestCube.cpp)

target_compile_options(engine PRIVATE -DUNICODE -DENGINE_DLL)

target_link_libraries(engine
        PRIVATE
        glm::glm
        libDaScript

        # Vulkan::Vulkan
        # d3d12.lib
        # dxgi.lib
        # dxguid.lib

        # glfw
        #Threads::Threads
        Diligent-GraphicsEngineD3D11-shared
        Diligent-GraphicsEngineOpenGL-shared
        Diligent-GraphicsEngineD3D12-shared
        Diligent-GraphicsEngineVk-shared
        Diligent-GraphicsTools
        Diligent-RenderStateNotation
       # Diligent-Imgui

        # do something
        ${THIRD_PARTY_DIR}/DiligentCore/Common/Debug/Diligent-Common
        ${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsTools/Debug/Diligent-GraphicsTools
        ${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsAccessories/Debug/Diligent-GraphicsAccessories
        ${THIRD_PARTY_DIR}/DiligentCore/Primitives/Debug/Diligent-Primitives
        ${THIRD_PARTY_DIR}/DiligentCore/Platforms/Win32/Debug/Diligent-Win32Platform
        ${THIRD_PARTY_DIR}/DiligentCore/Platforms/Basic/Debug/Diligent-BasicPlatform
        ${THIRD_PARTY_DIR}/DiligentTools/RenderStateNotation/Debug/Diligent-RenderStateNotation
        )

copy_required_dlls(engine)

include_directories(
    "${CMAKE_SOURCE_DIR}/engine/src"
)

target_include_directories(engine
        PRIVATE
        "${THIRD_PARTY_DIR}"
        "${THIRD_PARTY_DIR}/imgui"
        "${THIRD_PARTY_DIR}/DiligentCore/Common/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Platforms/Basic/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsTools/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsEngineD3D11/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsEngineOpenGL/interface"
        "${THIRD_PARTY_DIR}/DiligentCore/Graphics/GraphicsEngineVulkan/interface"
        "${THIRD_PARTY_DIR}/DiligentTools/Graphics/Imgui/interface"
        )

target_compile_definitions(engine PRIVATE UNICODE _UNICODE)

target_compile_features(engine
        PRIVATE
        cxx_std_17
        )
        
      

ADD_DEPENDENCIES(engine daScript)
SETUP_CPP11(engine)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
