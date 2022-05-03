cmake_minimum_required (VERSION 3.16)
project(engine VERSION 0.1.0)

set(DAS_BUILD_TUTORIAL NO)
set(DAS_BUILD_TEST NO)
set(DAS_BUILD_PROFILE NO)
set(DAS_GLFW_DISABLED YES)

# Add dascript
# include(ExternalProject)
# ExternalProject_Add(
#     libDaScript
#     SOURCE_DIR daScript
#     PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libDaScript
#     #CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${DAS_SFML_DIR}/libsfml -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES} -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=true
#     #BINARY_DIR ${DAS_SFML_DIR}/libsfml/build
# )

add_subdirectory(daScript)
include_directories(daScript/include)

include(CTest)
enable_testing()

add_executable(engine engine/engine.cpp)

TARGET_LINK_LIBRARIES(engine 
libDaScript 
#Threads::Threads
)
ADD_DEPENDENCIES(engine daScript)
SETUP_CPP11(engine)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
