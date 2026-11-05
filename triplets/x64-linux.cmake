# Overlay of the builtin x64-linux triplet. The Ubuntu 26.04 toolchain defaults to x86-64-v3;
# the flags must be in the triplet (not env vars) to be part of the vcpkg ABI hash, so v3
# binary-cache entries are never reused.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_C_FLAGS "-march=x86-64 -mtune=generic")
set(VCPKG_CXX_FLAGS "-march=x86-64 -mtune=generic")
