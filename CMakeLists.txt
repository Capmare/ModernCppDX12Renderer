cmake_minimum_required(VERSION 3.31.6)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(ModernCppDX12Renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE SRC_MODULES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/*.ixx"
        "${CMAKE_SOURCE_DIR}/src/*.cppm"
)

add_executable(ModernCppDX12Renderer WIN32)

# 1) normal .cpp sources
target_sources(ModernCppDX12Renderer PRIVATE
        ${SRC_FILES}
)

# 2) module interface units
target_sources(ModernCppDX12Renderer PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ${SRC_MODULES}
)

target_link_libraries(ModernCppDX12Renderer PRIVATE
        d3d12 dxgi d3dcompiler dxguid dbghelp
)

if (MSVC)
    # Force x64 visibility for Windows headers during scanDependencies
    target_compile_definitions(ModernCppDX12Renderer PRIVATE _AMD64_ _M_AMD64 WIN64 WIN32_LEAN_AND_MEAN NOMINMAX)

    # ---- Manual std module build (Ninja-safe) ----
    set(STL_MODULE_DIR "$ENV{VCToolsInstallDir}/modules")
    set(STD_IXX "${STL_MODULE_DIR}/std.ixx")

    set(STD_OUT_DIR "${CMAKE_BINARY_DIR}/stl_modules")
    file(MAKE_DIRECTORY "${STD_OUT_DIR}")

    set(STD_IFC "${STD_OUT_DIR}/std.ifc")
    set(STD_OBJ "${STD_OUT_DIR}/std.obj")

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(STD_RTL "/MDd")
    else()
        set(STD_RTL "/MD")
    endif()

    add_custom_command(
            OUTPUT "${STD_IFC}" "${STD_OBJ}"
            COMMAND "${CMAKE_CXX_COMPILER}"
            /nologo /std:c++latest /EHsc /experimental:module
            ${STD_RTL}
            /c
            "/ifcOutput${STD_IFC}"
            "/Fo${STD_OBJ}"
            "${STD_IXX}"
            DEPENDS "${STD_IXX}"
            VERBATIM
    )

    add_custom_target(build_std_module DEPENDS "${STD_IFC}" "${STD_OBJ}")
    add_dependencies(ModernCppDX12Renderer build_std_module)

    target_compile_options(ModernCppDX12Renderer PRIVATE
            /std:c++latest
            /experimental:module
            /reference
            "std=${STD_IFC}"
    )

    target_sources(ModernCppDX12Renderer PRIVATE "${STD_OBJ}")
endif()