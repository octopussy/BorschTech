cmake_minimum_required(VERSION 3.16)
project(engine VERSION 0.1.0)

# find_package(Vulkan REQUIRED FATAL_ERROR) # error

set(CMAKE_CONFIGURATION_TYPES "Release" "Debug")

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

# Add dascript
# include(ExternalProject)
# ExternalProject_Add(
#     libDaScript
#     SOURCE_DIR daScript
#     PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libDaScript
#     #CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${DAS_SFML_DIR}/libsfml -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES} -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=true
#     #BINARY_DIR ${DAS_SFML_DIR}/libsfml/build
# )

# Diligent
add_subdirectory(DiligentCore)
include_directories(DiligentCore)
add_subdirectory(DiligentTools)
include_directories(DiligentTools)
#include_directories(DiligentCore/Common/Interface)

# daScript 
set(DAS_BUILD_TUTORIAL NO)
set(DAS_BUILD_TEST NO)
set(DAS_BUILD_PROFILE NO)
set(DAS_GLFW_DISABLED YES)
add_subdirectory(3rdparty/daScript)
include_directories(3rdparty/daScript/include)

# GLFW
set(GLFW_DIR 3rdparty/glfw) # Set this to point to an up-to-date GLFW repo
set(GLFW_LIBRARIES ${GLFW_DIR}/glfw/lib/glfw3.lib)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
#add_subdirectory(${GLFW_DIR} binary_dir EXCLUDE_FROM_ALL)
add_subdirectory(${GLFW_DIR})
include_directories(${GLFW_DIR}/include)

# GLM
set(GLM_DIR 3rdparty/glm)
include_directories(${GLM_DIR})

# imgui
set(IMGUI_DIR 3rdparty/imgui)

include(CTest)
enable_testing()

add_executable(engine WIN32
        # ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        # ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    #    ${IMGUI_DIR}/imgui.cpp
    #    ${IMGUI_DIR}/imgui_draw.cpp
    #    ${IMGUI_DIR}/imgui_demo.cpp
    #    ${IMGUI_DIR}/imgui_tables.cpp
    #    ${IMGUI_DIR}/imgui_widgets.cpp

        #engine/src/main.cpp
        engine/src/Application.cpp
        #engine/src/GLFWDemo.cpp
        #engine/src/Game.cpp
       # engine/src/imGui.cpp
        # engine/src/imgul_vulkan_test.cpp
      #  engine/src/main.h
      #  engine/src/vulkan_test.cpp
      #  engine/src/Tutorial01_HelloTriangle.cpp
      #   engine/src/DiligentTest.cpp
        )

target_compile_options(engine PRIVATE -DUNICODE -DENGINE_DLL)

target_link_libraries(engine
        PRIVATE
        libDaScript
        #Vulkan::Vulkan
        glfw
        #Threads::Threads
        Diligent-GraphicsEngineD3D11-shared
        Diligent-GraphicsEngineOpenGL-shared
        Diligent-GraphicsEngineD3D12-shared
        Diligent-GraphicsEngineVk-shared
        Diligent-GraphicsTools
        Diligent-RenderStateNotation

        DiligentCore/Common/Debug/Diligent-Common
        DiligentCore/Graphics/GraphicsTools/Debug/Diligent-GraphicsTools
        DiligentCore/Graphics/GraphicsAccessories/Debug/Diligent-GraphicsAccessories
        DiligentCore/Primitives/Debug/Diligent-Primitives
        DiligentCore/Platforms/Win32/Debug/Diligent-Win32Platform
        DiligentCore/Platforms/Basic/Debug/Diligent-BasicPlatform
        DiligentTools/RenderStateNotation/Debug/Diligent-RenderStateNotation
        )

copy_required_dlls(engine)

target_include_directories(engine
        PRIVATE
        "${CMAKE_SOURCE_DIR}/3rdparty"
        "${CMAKE_SOURCE_DIR}/3rdparty/imgui"
        "${CMAKE_SOURCE_DIR}/DiligentCore/Graphics/GraphicsTools/interface"
        "${CMAKE_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineD3D11/interface"
        "${CMAKE_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
        "${CMAKE_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineOpenGL/interface"
        "${CMAKE_SOURCE_DIR}/DiligentCore/Graphics/GraphicsEngineVulkan/interface"
        # "${CMAKE_SOURCE_DIR}/src"
        )


target_compile_features(engine
        PRIVATE
        cxx_std_17
        )

ADD_DEPENDENCIES(engine daScript)
SETUP_CPP11(engine)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
