# MinGW-w64 cross toolchain for building the Abaser Clicker exe from Linux.
# Use: cmake -S exe -B build -DCMAKE_TOOLCHAIN_FILE=exe/mingw-toolchain.cmake
#
# Setting CMAKE_SYSTEM_NAME here matters beyond documentation: it makes CMake
# load its Windows-GNU platform rules, which is what applies -mwindows to
# WIN32 executables (GUI subsystem) and friends.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Debian/Ubuntu place the sysroot here; the compilers find it by default,
# but make it explicit for good measure.
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
