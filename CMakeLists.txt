cmake_minimum_required(VERSION 3.31)
project(ModernCppDX12Renderer)

set(CMAKE_CXX_STANDARD 23)


file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
add_executable(ModernCppDX12Renderer WIN32 ${SRC_FILES})

target_precompile_headers(ModernCppDX12Renderer PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")

target_link_libraries(ModernCppDX12Renderer PRIVATE
        d3d12
        dxgi
        d3dcompiler
)

target_precompile_headers(ModernCppDX12Renderer PRIVATE src/pch.h)