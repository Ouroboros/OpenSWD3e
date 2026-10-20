# Cross toolchain: clang-cl + lld-link from WSL.
# Requires WINSYSROOT pointing to an MSVC CRT + Windows SDK layout (e.g. produced by xwin).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)

if(NOT DEFINED ENV{WINSYSROOT})
  message(FATAL_ERROR "WINSYSROOT is not set; Windows SDK/MSVC sysroot is required for the windows preset")
endif()
set(WINSYSROOT "$ENV{WINSYSROOT}")
set(CMAKE_C_FLAGS_INIT "/winsysroot ${WINSYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "/winsysroot ${WINSYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "/winsysroot:${WINSYSROOT}")
