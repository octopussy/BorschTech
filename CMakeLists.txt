cmake_minimum_required(VERSION 3.16)
project(engine VERSION 0.1.0)

find_package(Vulkan REQUIRED FATAL_ERROR) # error

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

# daScript 
set(DAS_BUILD_TUTORIAL NO)
set(DAS_BUILD_TEST NO)
set(DAS_BUILD_PROFILE NO)
set(DAS_GLFW_DISABLED YES)
add_subdirectory(_thirdparty/daScript)
include_directories(_thirdparty/daScript/include)

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

# imgui
set(IMGUI_DIR 3rdparty/imgui)

include(CTest)
enable_testing()

add_executable(engine
        # ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        # ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp

        engine/src/main.cpp
        engine/src/imGui.cpp
        engine/src/imgul_vulkan_test.cpp
        engine/src/imgui_vulkan_test.h)

target_link_libraries(engine
        libDaScript
        Vulkan::Vulkan
        glfw
        #Threads::Threads
        )

target_include_directories(engine
        PRIVATE
        "${CMAKE_SOURCE_DIR}/3rdparty"
        "${CMAKE_SOURCE_DIR}/3rdparty/imgui"
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
