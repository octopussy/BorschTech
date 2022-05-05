
rd /Q /S _intermediate
mkdir _intermediate
cd _intermediate

set CMAKE_GENERATOR=Visual Studio 17 2022

cmake -G "%CMAKE_GENERATOR%" -DCMAKE_INSTALL_PREFIX=. ..\
