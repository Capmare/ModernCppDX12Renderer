cmake_minimum_required(VERSION 4.0.2)
project(ModernCppDX12Renderer)

set(CMAKE_CXX_STANDARD 26)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(ModernCppDX12Renderer WIN32 ${SRC_FILES})

target_precompile_headers(ModernCppDX12Renderer PRIVATE src/pch.h)

target_link_libraries(ModernCppDX12Renderer PRIVATE
        d3d12
        dxgi
        d3dcompiler
        dxguid
        dbghelp
)

if (MSVC)
    target_compile_options(ModernCppDX12Renderer PRIVATE
            $<$<CONFIG:Debug>:/Zi>
            $<$<CONFIG:RelWithDebInfo>:/Zi>
    )

    target_link_options(ModernCppDX12Renderer PRIVATE
            $<$<CONFIG:Debug>:/DEBUG>
            $<$<CONFIG:RelWithDebInfo>:/DEBUG>
    )

    set_property(TARGET ModernCppDX12Renderer PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_compile_definitions(ModernCppDX12Renderer PRIVATE
            $<$<CONFIG:Debug>:D3D12_ENABLE_DEBUG_LAYER>
    )
endif()
