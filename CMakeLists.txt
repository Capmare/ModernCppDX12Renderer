cmake_minimum_required(VERSION 4.0)
project(ModernCppDX12Renderer)

set(CMAKE_CXX_STANDARD 23)


file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(ModernCppDX12Renderer WIN32 ${SRC_FILES})

