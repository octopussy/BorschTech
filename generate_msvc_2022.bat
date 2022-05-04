
rd /Q /S build
mkdir build
cd build

set CMAKE_GENERATOR=Visual Studio 17 2022

cmake -G "%CMAKE_GENERATOR%" -DCMAKE_INSTALL_PREFIX=. ..\
