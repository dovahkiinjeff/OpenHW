cmake_minimum_required(VERSION 3.25)

project(HomeworldModern
    VERSION 1.0.0
    DESCRIPTION "Native Windows modernization of the Homeworld engine"
    LANGUAGES C CXX)

include(CTest)
if(WIN32)
    add_executable(OpenHWSetupBootstrap WIN32 EXCLUDE_FROM_ALL tools/release_bootstrap.cpp)
    target_compile_features(OpenHWSetupBootstrap PRIVATE cxx_std_17)
    target_compile_definitions(OpenHWSetupBootstrap PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(OpenHWSetupBootstrap PRIVATE comctl32 bcrypt crypt32)
    add_executable(OpenHWPatchBootstrap WIN32 EXCLUDE_FROM_ALL tools/release_bootstrap.cpp)
    target_compile_features(OpenHWPatchBootstrap PRIVATE cxx_std_17)
    target_compile_definitions(OpenHWPatchBootstrap PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN OPENHW_PATCH_INSTALLER)
    target_link_libraries(OpenHWPatchBootstrap PRIVATE comctl32 bcrypt crypt32 shell32 ole32)
    add_executable(bc6h_encode EXCLUDE_FROM_ALL tools/bc6h_encode.cpp)
    target_compile_features(bc6h_encode PRIVATE cxx_std_17)
    if(MSVC)
        set_property(TARGET OpenHWSetupBootstrap PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET OpenHWPatchBootstrap PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
if(BUILD_TESTING AND WIN32)
    add_executable(ship_authoring_tests tests/ShipAuthoringTests.cpp
        src/Game/ShipAuthoring.c src/Game/ShipAuthorText.c src/Modern/ShipAuthoringObj.cpp)
    target_include_directories(ship_authoring_tests PRIVATE src/Game src/Modern)
    target_compile_features(ship_authoring_tests PRIVATE cxx_std_17)
    target_compile_definitions(ship_authoring_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    # SDL2 target is resolved after find_package below.
endif()

if(NOT WIN32)
    message(FATAL_ERROR "The modern CMake target currently supports 64-bit Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "HomeworldModern requires an x64 generator/toolchain.")
endif()

option(HW_ENABLE_MOVIES "Enable FFmpeg-backed Bink cutscene playback" ON)
option(HW_ENABLE_NETWORK "Enable SDL_net-backed LAN networking" OFF)
option(HW_ENABLE_D3D12_DIAGNOSTICS "Probe the active GPU for D3D12/DXR support at startup" ON)
option(HW_ENABLE_D3D12_BACKEND "Use the D3D12 DXGI visible presentation bridge" ON)
option(HW_ENABLE_D3D12_NATIVE_RASTER "Replace the legacy OpenGL context with the native D3D12 fixed-function compatibility rasterizer" ON)
option(HW_ENABLE_ASAN "Enable MSVC AddressSanitizer for native crash diagnosis" OFF)
option(HW_ENABLE_DIAGNOSTICS "Compile engine logging, profiling, captures and crash reports" ON)
add_compile_definitions(HW_ENABLE_DIAGNOSTICS=$<BOOL:${HW_ENABLE_DIAGNOSTICS}>)

if(HW_ENABLE_ASAN AND NOT MSVC)
    message(FATAL_ERROR "HW_ENABLE_ASAN currently requires the MSVC toolchain.")
endif()

find_package(SDL2 CONFIG REQUIRED)
if(TARGET ship_authoring_tests)
    target_link_libraries(ship_authoring_tests PRIVATE SDL2::SDL2)
    add_test(NAME ship_authoring COMMAND ship_authoring_tests)
endif()
if(HW_ENABLE_D3D12_NATIVE_RASTER)
    if(NOT HW_ENABLE_D3D12_BACKEND)
        message(FATAL_ERROR "HW_ENABLE_D3D12_NATIVE_RASTER requires HW_ENABLE_D3D12_BACKEND.")
    endif()
else()
    find_package(OpenGL REQUIRED)
endif()

if(HW_ENABLE_MOVIES)
    find_package(FFMPEG REQUIRED)
endif()

if(HW_ENABLE_NETWORK)
    find_package(SDL2_net CONFIG REQUIRED)
endif()

# The vcpkg winflexbison port installs host tools outside the ordinary PATH.
# Help CMake's standard FindBISON/FindFLEX modules locate them.
set(_hw_tool_hints)
if(DEFINED VCPKG_INSTALLED_DIR)
    if(NOT DEFINED VCPKG_HOST_TRIPLET)
        set(VCPKG_HOST_TRIPLET "x64-windows")
    endif()
    list(APPEND _hw_tool_hints
        "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/winflexbison"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/winflexbison")
endif()

find_program(_hw_bison NAMES win_bison bison HINTS ${_hw_tool_hints})
find_program(_hw_flex NAMES win_flex flex HINTS ${_hw_tool_hints})
if(_hw_bison)
    set(BISON_EXECUTABLE "${_hw_bison}" CACHE FILEPATH "Bison executable" FORCE)
endif()
if(_hw_flex)
    set(FLEX_EXECUTABLE "${_hw_flex}" CACHE FILEPATH "Flex executable" FORCE)
endif()

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(HW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(hw_startup_progress STATIC
    "${HW_SOURCE_DIR}/src/Modern/WindowsStartupProgress.cpp")
target_compile_features(hw_startup_progress PUBLIC cxx_std_17)
set(HW_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${HW_GENERATED_DIR}/kas2c")
file(MAKE_DIRECTORY "${HW_GENERATED_DIR}/Game")
file(MAKE_DIRECTORY "${HW_GENERATED_DIR}/Missions")

BISON_TARGET(KasParser
    "${HW_SOURCE_DIR}/tools/kas2c/parser.y"
    "${HW_GENERATED_DIR}/kas2c/parser.c"
    DEFINES_FILE "${HW_GENERATED_DIR}/kas2c/parser.h")
FLEX_TARGET(KasLexer
    "${HW_SOURCE_DIR}/tools/kas2c/lexer.l"
    "${HW_GENERATED_DIR}/kas2c/lexer.c"
    COMPILE_FLAGS "-i --wincompat")
ADD_FLEX_BISON_DEPENDENCY(KasLexer KasParser)

add_executable(kas2c
    "${HW_SOURCE_DIR}/tools/kas2c/kas2c.c"
    ${BISON_KasParser_OUTPUTS}
    ${FLEX_KasLexer_OUTPUTS})
target_include_directories(kas2c PRIVATE
    "${HW_SOURCE_DIR}/tools/kas2c"
    "${HW_SOURCE_DIR}/src/Game"
    "${HW_GENERATED_DIR}/kas2c")
target_link_libraries(kas2c PRIVATE SDL2::SDL2)
target_compile_definitions(kas2c PRIVATE
    _CRT_NONSTDC_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if(MSVC)
    set(HW_PREPROCESSOR_MODE MSVC)
    target_compile_options(kas2c PRIVATE /W3 /wd4996)
else()
    set(HW_PREPROCESSOR_MODE GNU)
endif()

set(HW_PREPROCESS_SCRIPT "${HW_SOURCE_DIR}/cmake/Preprocess.cmake")

function(hw_add_kas output_name input_file)
    set(_kp "${HW_GENERATED_DIR}/Missions/${output_name}.kp")
    set(_c "${HW_GENERATED_DIR}/Missions/${output_name}.c")
    set(_h "${HW_GENERATED_DIR}/Missions/${output_name}.h")
    set(_func "${HW_GENERATED_DIR}/Missions/${output_name}.func.c")

    add_custom_command(
        OUTPUT "${_c}" "${_h}" "${_func}"
        COMMAND "${CMAKE_COMMAND}"
            "-DHW_COMPILER=${CMAKE_C_COMPILER}"
            "-DHW_MODE=${HW_PREPROCESSOR_MODE}"
            "-DHW_INPUT=${input_file}"
            "-DHW_OUTPUT=${_kp}"
            -P "${HW_PREPROCESS_SCRIPT}"
        COMMAND $<TARGET_FILE:kas2c> "${_kp}" "${_c}" "${_h}" "${_func}"
        DEPENDS
            kas2c
            "${input_file}"
            "${HW_SOURCE_DIR}/src/Missions/SinglePlayer/Swarmer.kash"
            "${HW_SOURCE_DIR}/src/Missions/SinglePlayer/Utilities.kash"
            "${HW_PREPROCESS_SCRIPT}"
        COMMENT "Generating ${output_name} mission sources"
        VERBATIM)

    set(HW_GENERATED_MISSION_SOURCES
        ${HW_GENERATED_MISSION_SOURCES} "${_c}" "${_h}" "${_func}"
        PARENT_SCOPE)
endfunction()

set(HW_SINGLE_PLAYER_MISSIONS
    Mission01 Mission02 Mission03 Mission04 Mission05 Mission05_OEM Mission06
    Mission07 Mission08 Mission09 Mission10 Mission11 Mission12 Mission13
    Mission14 Mission15 Mission16)

foreach(_mission IN LISTS HW_SINGLE_PLAYER_MISSIONS)
    hw_add_kas("${_mission}"
        "${HW_SOURCE_DIR}/src/Missions/SinglePlayer/${_mission}.kas")
endforeach()
hw_add_kas("Tutorial1"
    "${HW_SOURCE_DIR}/src/Missions/Tutorials/Tutorial1.kas")

set(HW_WRAPPED_FUNCTIONS "${HW_GENERATED_DIR}/Game/wrapped_functions.h")
add_custom_command(
    OUTPUT "${HW_WRAPPED_FUNCTIONS}"
    COMMAND "${CMAKE_COMMAND}"
        "-DHW_COMPILER=${CMAKE_C_COMPILER}"
        "-DHW_MODE=${HW_PREPROCESSOR_MODE}"
        "-DHW_INPUT=${HW_SOURCE_DIR}/src/Game/functions.h"
        "-DHW_OUTPUT=${HW_WRAPPED_FUNCTIONS}"
        -P "${HW_PREPROCESS_SCRIPT}"
    DEPENDS
        "${HW_SOURCE_DIR}/src/Game/functions.h"
        "${HW_SOURCE_DIR}/src/Game/wrap.h"
        "${HW_SOURCE_DIR}/src/Game/wrap_types.h"
        "${HW_PREPROCESS_SCRIPT}"
    COMMENT "Generating wrapped effect functions"
    VERBATIM)

file(GLOB HW_GAME_SOURCES CONFIGURE_DEPENDS "${HW_SOURCE_DIR}/src/Game/*.c")
add_executable(remastered_weapon_fx_data_tests "${HW_SOURCE_DIR}/tests/RemasteredWeaponFXDataTests.cpp")
target_include_directories(remastered_weapon_fx_data_tests PRIVATE "${HW_SOURCE_DIR}/src/Game")
target_compile_features(remastered_weapon_fx_data_tests PRIVATE cxx_std_17)
set(HW_MESH_SOURCE "${HW_SOURCE_DIR}/src/Game/Mesh.c")
if(NOT EXISTS "${HW_MESH_SOURCE}")
    message(FATAL_ERROR "Required legacy mesh implementation is missing: ${HW_MESH_SOURCE}")
endif()
# Mesh.c owns the legacy mesh renderer/loader and several renderer globals.
# Keep it out of the wildcard list and add it explicitly to the target below.
# This avoids Visual Studio project regeneration ever dropping Mesh.obj while
# other Game sources still reference meshLoad/meshRender/etc.
list(REMOVE_ITEM HW_GAME_SOURCES "${HW_MESH_SOURCE}")
# Retire the modern harvestable DustCloud replacement. Legacy Clouds.c owns
# harvestable DustCloud/GasCloud rendering again.
list(REMOVE_ITEM HW_GAME_SOURCES "${HW_SOURCE_DIR}/src/Game/ModernDustCloud.c")

file(GLOB HW_SDL_SOURCES CONFIGURE_DEPENDS "${HW_SOURCE_DIR}/src/SDL/*.c")

file(GLOB HW_SHIP_SOURCES CONFIGURE_DEPENDS "${HW_SOURCE_DIR}/src/Ships/*.c")

set(HW_THIRD_PARTY_SOURCES
    "${HW_SOURCE_DIR}/src/ThirdParty/CRC/CRC32.c"
    "${HW_SOURCE_DIR}/src/ThirdParty/LZSS/BitIO.c"
    "${HW_SOURCE_DIR}/src/ThirdParty/LZSS/LZSS.c")

set(HW_MODERN_SOURCES
    "${HW_SOURCE_DIR}/src/Modern/MissionLoadProgress.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundBank.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredFxGraph.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredFxPlayback.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredFxResources.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundPlayback.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredVisualAsset.cpp"
    "${HW_SOURCE_DIR}/src/Modern/RemasteredVisualPackage.cpp"
    "${HW_SOURCE_DIR}/src/Modern/OpenHWArchive.cpp"
    "${HW_SOURCE_DIR}/src/Modern/OpenHWMods.cpp"
    "${HW_SOURCE_DIR}/src/Modern/ShipAuthoringObj.cpp"
    "${HW_SOURCE_DIR}/src/Modern/WindowsDiagnostics.cpp"
    "${HW_SOURCE_DIR}/src/Modern/WindowsInstall.cpp"
    "${HW_SOURCE_DIR}/src/Modern/WindowsSplash.cpp")
if(HW_ENABLE_D3D12_DIAGNOSTICS OR HW_ENABLE_D3D12_BACKEND)
    list(APPEND HW_MODERN_SOURCES
        "${HW_SOURCE_DIR}/src/Modern/ModernGraphics.cpp")
endif()

if(HW_ENABLE_D3D12_BACKEND)
    list(APPEND HW_MODERN_SOURCES
        "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.cpp"
        "${HW_SOURCE_DIR}/src/Modern/ModernDlaa.cpp"
        "${HW_SOURCE_DIR}/src/Modern/ModernFrameGeneration.cpp"
        "${HW_SOURCE_DIR}/src/Modern/FGEffectGuides.cpp"
        "${HW_SOURCE_DIR}/src/Modern/FGZoomCapture.cpp"
        "${HW_SOURCE_DIR}/src/Modern/ModernUpscaler.cpp")
    if(HW_ENABLE_D3D12_NATIVE_RASTER)
        list(APPEND HW_MODERN_SOURCES
            "${HW_SOURCE_DIR}/src/Modern/LegacyD3D12GL.cpp")
    endif()

    set(HW_STREAMLINE_ROOT "${HW_SOURCE_DIR}/.deps/streamline" CACHE PATH
        "NVIDIA Streamline SDK root")
    find_path(HW_STREAMLINE_INCLUDE_DIR sl.h
        HINTS "${HW_STREAMLINE_ROOT}/include" NO_DEFAULT_PATH)
    find_library(HW_STREAMLINE_INTERPOSER_LIBRARY sl.interposer
        HINTS "${HW_STREAMLINE_ROOT}/lib/x64" NO_DEFAULT_PATH)
    set(HW_STREAMLINE_RUNTIME_DIR "${HW_STREAMLINE_ROOT}/bin/x64")
    foreach(_hw_streamline_file IN ITEMS
            sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll)
        if(NOT EXISTS "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_streamline_file}")
            message(FATAL_ERROR
                "NVIDIA Streamline runtime '${_hw_streamline_file}' is missing. "
                "Run Windows/bootstrap.ps1 to install the signed v2.12.0 SDK.")
        endif()
    endforeach()
    # DLSS Ray Reconstruction is a separate Streamline feature/runtime.
    # Require the signed production payload instead of producing a build that
    # advertises RR but silently reports RUNTIME MISSING at launch.
    set(HW_STREAMLINE_RR_RUNTIME_NAMES sl.dlss_d.dll nvngx_dlssd.dll)
    set(HW_STREAMLINE_RR_RUNTIME_PATHS)
    foreach(_hw_streamline_rr_file IN LISTS HW_STREAMLINE_RR_RUNTIME_NAMES)
        set(_hw_rr_path "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_streamline_rr_file}")
        if(NOT EXISTS "${_hw_rr_path}")
            message(FATAL_ERROR
                "NVIDIA DLSS Ray Reconstruction runtime '${_hw_streamline_rr_file}' is missing from the normalized Streamline runtime. "
                "Run Windows/bootstrap.ps1 to install the production RR runtime.")
        endif()
        list(APPEND HW_STREAMLINE_RR_RUNTIME_PATHS "${_hw_rr_path}")
    endforeach()
    set(HW_STREAMLINE_FG_RUNTIME_NAMES
        sl.dlss_g.dll nvngx_dlssg.dll sl.reflex.dll sl.pcl.dll)
    foreach(_hw_streamline_fg_file IN LISTS HW_STREAMLINE_FG_RUNTIME_NAMES)
        if(NOT EXISTS "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_streamline_fg_file}")
            message(FATAL_ERROR "NVIDIA frame-generation runtime missing: ${_hw_streamline_fg_file}")
        endif()
    endforeach()
    if(NOT HW_STREAMLINE_INCLUDE_DIR OR NOT HW_STREAMLINE_INTERPOSER_LIBRARY)
        message(FATAL_ERROR
            "NVIDIA Streamline SDK v2.12.0 is missing. Run "
            "Windows/bootstrap.ps1 before configuring the build.")
    endif()
    set(HW_FIDELITYFX_ROOT "${HW_SOURCE_DIR}/.deps/fidelityfx" CACHE PATH "AMD FidelityFX SDK root")
    set(HW_XESS_ROOT "${HW_SOURCE_DIR}/.deps/xess" CACHE PATH "Intel XeSS SDK root")
    set(HW_FIDELITYFX_LOADER_LIBRARY "${HW_FIDELITYFX_ROOT}/lib/amd_fidelityfx_loader_dx12.lib")
    set(HW_FIDELITYFX_FG_RUNTIME "${HW_FIDELITYFX_ROOT}/bin/amd_fidelityfx_framegeneration_dx12.dll")
    set(HW_XESS_LIBRARY "${HW_XESS_ROOT}/lib/libxess.lib")
    foreach(_hw_vendor_file IN ITEMS
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/api/include/ffx_api.hpp"
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/upscalers/include/ffx_upscale.hpp"
            "${HW_FIDELITYFX_LOADER_LIBRARY}"
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/framegeneration/include/ffx_framegeneration.h"
            "${HW_FIDELITYFX_FG_RUNTIME}"
            "${HW_XESS_ROOT}/include/xess/xess_d3d12.h"
            "${HW_XESS_LIBRARY}")
        if(NOT EXISTS "${_hw_vendor_file}")
            message(FATAL_ERROR "Cross-vendor reconstruction dependency missing: ${_hw_vendor_file}. Run Windows/bootstrap.ps1.")
        endif()
    endforeach()
    # The official vcpkg package exports the compiler path. Keep a direct
    # tool-directory search as a fallback for older package revisions.
    find_package(directx-dxc CONFIG QUIET)
    if(DIRECTX_DXC_TOOL)
        set(HW_DXC_EXECUTABLE "${DIRECTX_DXC_TOOL}")
    endif()
    set(_hw_dxc_hints)
    if(DEFINED VCPKG_INSTALLED_DIR)
        list(APPEND _hw_dxc_hints
            "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/directx-dxc"
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/directx-dxc")
    endif()
    find_program(HW_DXC_EXECUTABLE NAMES dxc dxc.exe
        HINTS ${_hw_dxc_hints})
    if(NOT HW_DXC_EXECUTABLE)
        message(FATAL_ERROR
            "The DirectX Shader Compiler (dxc) was not found. Re-run "
            "Windows/bootstrap.ps1 so vcpkg can install directx-dxc.")
    endif()

    file(MAKE_DIRECTORY "${HW_GENERATED_DIR}/Modern")
    set(HW_DXR_SHADER_HEADER
        "${HW_GENERATED_DIR}/Modern/ModernRaytracingShader.h")
    add_custom_command(
        OUTPUT "${HW_DXR_SHADER_HEADER}"
        COMMAND "${HW_DXC_EXECUTABLE}"
            -T lib_6_3
            -HV 2021
            -O3
            -Fh "${HW_DXR_SHADER_HEADER}"
            -Vn g_hwModernRaytracingLibrary
            "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.hlsl"
        DEPENDS "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.hlsl"
        COMMENT "Compiling the DXR path-tracing shader library"
        VERBATIM)
    list(APPEND HW_MODERN_SOURCES "${HW_DXR_SHADER_HEADER}")
    set(HW_DXR_FEEDBACK_SHADER_HEADER
        "${HW_GENERATED_DIR}/Modern/ModernRaytracingFeedbackShader.h")
    add_custom_command(
        OUTPUT "${HW_DXR_FEEDBACK_SHADER_HEADER}"
        COMMAND "${HW_DXC_EXECUTABLE}"
            -T lib_6_3 -D HW_MATERIAL_SAMPLER_FEEDBACK=1
            -HV 2021 -O3
            -Fh "${HW_DXR_FEEDBACK_SHADER_HEADER}"
            -Vn g_hwModernRaytracingFeedbackLibrary
            "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.hlsl"
        DEPENDS "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.hlsl"
        COMMENT "Compiling optional sampler-feedback DXR library"
        VERBATIM)
    list(APPEND HW_MODERN_SOURCES "${HW_DXR_FEEDBACK_SHADER_HEADER}")
    set(HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER
        "${HW_GENERATED_DIR}/Modern/MaterialMipFeedbackShader.h")
    add_custom_command(OUTPUT "${HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER}"
        COMMAND "${HW_DXC_EXECUTABLE}" -T cs_6_5 -E MaterialMipFeedbackCS
            -HV 2021 -O3 -Fh "${HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER}"
            -Vn g_hwMaterialMipFeedbackShader
            "${HW_SOURCE_DIR}/src/Modern/MaterialMipFeedback.hlsl"
        DEPENDS "${HW_SOURCE_DIR}/src/Modern/MaterialMipFeedback.hlsl"
        COMMENT "Compiling hardware sampler-feedback collection" VERBATIM)
    list(APPEND HW_MODERN_SOURCES "${HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER}")
    set(HW_DUST_SHADER_SOURCE "${HW_GENERATED_DIR}/Modern/DustSunVisibility.hlsl")
    set(HW_DUST_SHADER_HEADER "${HW_GENERATED_DIR}/Modern/DustSunVisibilityShader.h")
    add_custom_command(OUTPUT "${HW_DUST_SHADER_HEADER}"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
            "${HW_SOURCE_DIR}/tools/extract_presenter_shader.ps1"
            -Source "${HW_SOURCE_DIR}/src/Modern/ModernGraphics.cpp"
            -Output "${HW_DUST_SHADER_SOURCE}"
        COMMAND "${HW_DXC_EXECUTABLE}" -T cs_6_5 -E BuildDustSunColumnsCS
            -D DUST_FROXEL_COMPUTE=1 -D DUST_RAY_QUERY=1 -HV 2021 -O3
            -Fh "${HW_DUST_SHADER_HEADER}" -Vn g_hwDustSunVisibilityShader
            "${HW_DUST_SHADER_SOURCE}"
        DEPENDS "${HW_SOURCE_DIR}/src/Modern/ModernGraphics.cpp"
            "${HW_SOURCE_DIR}/tools/extract_presenter_shader.ps1"
        COMMENT "Compiling geometric volumetric sunlight visibility" VERBATIM)
    list(APPEND HW_MODERN_SOURCES "${HW_DUST_SHADER_HEADER}")
    set_source_files_properties("${HW_SOURCE_DIR}/src/Modern/ModernGraphics.cpp"
        PROPERTIES OBJECT_DEPENDS "${HW_DUST_SHADER_HEADER}")
    set_source_files_properties(
        "${HW_SOURCE_DIR}/src/Modern/ModernRaytracing.cpp"
        PROPERTIES OBJECT_DEPENDS "${HW_DXR_SHADER_HEADER};${HW_DXR_FEEDBACK_SHADER_HEADER};${HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER}")
endif()

set_source_files_properties("${HW_SOURCE_DIR}/src/Game/ETG.c"
    PROPERTIES OBJECT_DEPENDS "${HW_WRAPPED_FUNCTIONS}")

# Keep a console subsystem for the first native revisions: the original engine
# writes critical diagnostics to stdout/stderr.  SDL_MAIN_HANDLED preserves the
# ordinary C main() entry point.
add_executable(homeworld
    ${HW_GAME_SOURCES}
    "${HW_MESH_SOURCE}"
    ${HW_SDL_SOURCES}
    ${HW_SHIP_SOURCES}
    ${HW_THIRD_PARTY_SOURCES}
    ${HW_MODERN_SOURCES}
    ${HW_GENERATED_MISSION_SOURCES}
    "${HW_WRAPPED_FUNCTIONS}")
set_source_files_properties("${HW_MESH_SOURCE}" PROPERTIES LANGUAGE C)
get_target_property(_hw_homeworld_sources homeworld SOURCES)
list(FIND _hw_homeworld_sources "${HW_MESH_SOURCE}" _hw_mesh_source_index)
if(_hw_mesh_source_index EQUAL -1)
    message(FATAL_ERROR "Mesh.c was not attached to the homeworld target")
endif()

target_include_directories(homeworld PRIVATE
    "${HW_SOURCE_DIR}/src/Game"
    "${HW_SOURCE_DIR}/src/SDL"
    "${HW_SOURCE_DIR}/src/Ships"
    "${HW_SOURCE_DIR}/src/Missions/SinglePlayer"
    "${HW_SOURCE_DIR}/src/Missions/Tutorials"
    "${HW_SOURCE_DIR}/src/ThirdParty/CRC"
    "${HW_SOURCE_DIR}/src/ThirdParty/LZSS"
    "${HW_SOURCE_DIR}/src/ThirdParty/Titan"
    "${HW_SOURCE_DIR}/src/ThirdParty/stb"
    "${HW_SOURCE_DIR}/src/Modern"
    "${HW_GENERATED_DIR}/Modern"
    "${HW_GENERATED_DIR}/Game"
    "${HW_GENERATED_DIR}/Missions")

if(HW_ENABLE_D3D12_BACKEND)
    add_library(hw_gpu_memory STATIC
        "${HW_SOURCE_DIR}/src/Modern/GpuMemoryAllocator.cpp"
        "${HW_SOURCE_DIR}/src/ThirdParty/D3D12MemoryAllocator/D3D12MemAlloc.cpp")
    target_include_directories(hw_gpu_memory PUBLIC "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(hw_gpu_memory PUBLIC cxx_std_17)
    target_compile_definitions(hw_gpu_memory PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(hw_gpu_memory PRIVATE d3d12 dxgi dxguid)
    add_library(hw_material_streaming STATIC
        "${HW_SOURCE_DIR}/src/Modern/SamplerFeedbackTexture.cpp")
    target_include_directories(hw_material_streaming PUBLIC "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(hw_material_streaming PUBLIC cxx_std_17)
    target_compile_definitions(hw_material_streaming PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(hw_material_streaming PRIVATE d3d12 dxgi)
    target_include_directories(homeworld PRIVATE "${HW_STREAMLINE_INCLUDE_DIR}")
    target_link_libraries(homeworld PRIVATE hw_gpu_memory hw_material_streaming)
    if(HW_ENABLE_D3D12_NATIVE_RASTER)
        # Lossless vertex identity remapping only; triangle order is unchanged.
        add_library(hw_meshoptimizer STATIC
            "${HW_SOURCE_DIR}/src/ThirdParty/meshoptimizer/src/indexgenerator.cpp"
            "${HW_SOURCE_DIR}/src/ThirdParty/meshoptimizer/src/allocator.cpp")
        target_compile_features(hw_meshoptimizer PUBLIC cxx_std_17)
        target_link_libraries(homeworld PRIVATE hw_meshoptimizer)
    endif()
endif()
if(HW_ENABLE_MOVIES)
    target_include_directories(homeworld PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(homeworld PRIVATE ${FFMPEG_LIBRARY_DIRS})
endif()

target_compile_features(homeworld PRIVATE c_std_11 cxx_std_17)
target_link_libraries(homeworld PRIVATE hw_startup_progress)
if(WIN32)
    find_package(directxtex CONFIG REQUIRED)
    target_link_libraries(homeworld PRIVATE Microsoft::DirectXTex)
    add_executable(ship_texture_dds "${HW_SOURCE_DIR}/tools/ship_texture_dds.cpp")
    target_compile_features(ship_texture_dds PRIVATE cxx_std_17)
    target_compile_definitions(ship_texture_dds PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(ship_texture_dds PRIVATE Microsoft::DirectXTex)
    if(HW_ENABLE_D3D12_BACKEND)
        add_executable(renderer_smoke EXCLUDE_FROM_ALL
            "${HW_SOURCE_DIR}/tools/renderer_smoke.cpp" "${HW_DXR_SHADER_HEADER}" "${HW_DXR_FEEDBACK_SHADER_HEADER}" "${HW_MATERIAL_MIP_FEEDBACK_SHADER_HEADER}" "${HW_DUST_SHADER_HEADER}")
        target_include_directories(renderer_smoke PRIVATE "${HW_GENERATED_DIR}/Modern")
        target_compile_features(renderer_smoke PRIVATE cxx_std_17)
        target_compile_definitions(renderer_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_definitions(renderer_smoke PRIVATE HW_DUST_SOURCE_PATH="${HW_DUST_SHADER_SOURCE}")
        target_link_libraries(renderer_smoke PRIVATE Microsoft::DirectXTex d3d12 dxgi d3dcompiler cabinet hw_gpu_memory hw_material_streaming hw_startup_progress)
        # Explicit-only: creates a private hidden HWND and runs real GPU/FG
        # work. Keep out of default builds and unattended CTest execution.
        add_executable(frame_generation_smoke EXCLUDE_FROM_ALL
            "${HW_SOURCE_DIR}/tools/frame_generation_smoke.cpp"
            "${HW_SOURCE_DIR}/src/Modern/ModernFrameGeneration.cpp"
            "${HW_SOURCE_DIR}/src/Modern/FGZoomCapture.cpp"
            "${HW_SOURCE_DIR}/src/Modern/ModernDLAA.cpp")
        target_compile_features(frame_generation_smoke PRIVATE cxx_std_17)
        target_compile_definitions(frame_generation_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_include_directories(frame_generation_smoke PRIVATE
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/api/include"
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/framegeneration/include"
            "${HW_STREAMLINE_INCLUDE_DIR}")
        target_link_libraries(frame_generation_smoke PRIVATE
            "${HW_STREAMLINE_INTERPOSER_LIBRARY}"
            "${HW_FIDELITYFX_LOADER_LIBRARY}" d3d12 dxgi d3dcompiler user32)
        foreach(_hw_fg_smoke_runtime IN ITEMS
                "${HW_FIDELITYFX_ROOT}/bin/amd_fidelityfx_loader_dx12.dll"
                "${HW_FIDELITYFX_FG_RUNTIME}"
                "${HW_STREAMLINE_RUNTIME_DIR}/sl.interposer.dll"
                "${HW_STREAMLINE_RUNTIME_DIR}/sl.common.dll"
                "${HW_STREAMLINE_RUNTIME_DIR}/sl.dlss.dll"
                "${HW_STREAMLINE_RUNTIME_DIR}/nvngx_dlss.dll"
                ${HW_STREAMLINE_RR_RUNTIME_PATHS})
            add_custom_command(TARGET frame_generation_smoke POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_hw_fg_smoke_runtime}" $<TARGET_FILE_DIR:frame_generation_smoke>)
        endforeach()
        foreach(_hw_fg_smoke_runtime IN LISTS HW_STREAMLINE_FG_RUNTIME_NAMES)
            add_custom_command(TARGET frame_generation_smoke POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_fg_smoke_runtime}"
                    $<TARGET_FILE_DIR:frame_generation_smoke>)
        endforeach()
        add_executable(remastered_scene_smoke EXCLUDE_FROM_ALL
            "${HW_SOURCE_DIR}/tools/remastered_scene_smoke.cpp")
        target_compile_features(remastered_scene_smoke PRIVATE cxx_std_17)
        target_compile_definitions(remastered_scene_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN HW_ENABLE_D3D12_NATIVE_RASTER SDL_MAIN_HANDLED)
        target_link_libraries(remastered_scene_smoke PRIVATE SDL2::SDL2 d3d11 d3d12 dxgi d3dcompiler hw_startup_progress)
        add_executable(dx11_renderer_smoke tools/dx11_renderer_smoke.cpp)
        target_include_directories(dx11_renderer_smoke PRIVATE src/Modern)
        target_compile_features(dx11_renderer_smoke PRIVATE cxx_std_17)
        target_compile_definitions(dx11_renderer_smoke PRIVATE HW_ENABLE_D3D12_NATIVE_RASTER NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(dx11_renderer_smoke PRIVATE SDL2::SDL2 Microsoft::DirectXTex d3d11 d3d12 dxgi d3dcompiler hw_gpu_memory hw_startup_progress)
        if(HW_ENABLE_D3D12_NATIVE_RASTER)
            target_link_libraries(remastered_scene_smoke PRIVATE hw_gpu_memory hw_meshoptimizer)
            target_link_libraries(dx11_renderer_smoke PRIVATE hw_meshoptimizer)
        endif()
    endif()
endif()
target_compile_definitions(homeworld PRIVATE
    SDL_MAIN_HANDLED
    HW_GAME_HOMEWORLD
    GENERIC_ETGCALLFUNCTION
    _X86_64
    _X86_64_FIX_ME
    _LINUX_FIX_ME
    _GNU_SOURCE
    WINDOWS
    _WINDOWS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_NONSTDC_NO_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    HW_MODERN_FORCE_LOD0
    HW_MODERN_DISABLE_NLIPS
    HW_MODERN_INFINITE_DRAW_DISTANCE
    "$<$<AND:$<BOOL:${HW_ENABLE_D3D12_DIAGNOSTICS}>,$<BOOL:${HW_ENABLE_DIAGNOSTICS}>>:HW_ENABLE_D3D12_DIAGNOSTICS>"
    "$<$<BOOL:${HW_ENABLE_D3D12_BACKEND}>:HW_ENABLE_D3D12_BACKEND>"
    "$<$<BOOL:${HW_ENABLE_D3D12_NATIVE_RASTER}>:HW_ENABLE_D3D12_NATIVE_RASTER>"
    "$<$<BOOL:${HW_ENABLE_NETWORK}>:HW_ENABLE_NETWORK>"
    "$<$<BOOL:${HW_ENABLE_MOVIES}>:HW_ENABLE_MOVIES>"
    "$<$<CONFIG:Debug>:HW_BUILD_FOR_DEBUGGING>"
    "$<$<NOT:$<CONFIG:Debug>>:HW_BUILD_FOR_DISTRIBUTION>")

if(MSVC)
    target_compile_options(homeworld PRIVATE
        "$<$<CONFIG:Release>:/Zi>"
        /W3
        /wd4013 /wd4047 /wd4090 /wd4113 /wd4133 /wd4244 /wd4267 /wd4996)
    if(HW_ENABLE_ASAN)
        target_compile_options(homeworld PRIVATE /fsanitize=address /Zi)
    endif()
    target_link_options(homeworld PRIVATE
        /DEBUG:FULL
        /MANIFEST:EMBED
        "/MANIFESTINPUT:${HW_SOURCE_DIR}/Windows/HomeworldModern.manifest")
    if(HW_ENABLE_ASAN)
        target_link_options(homeworld PRIVATE /INCREMENTAL:NO)
    endif()
endif()

target_link_libraries(homeworld PRIVATE
    SDL2::SDL2
    advapi32
    cabinet
    dbghelp
    comdlg32
    gdi32
    ole32
    shell32
    user32
    winmm)

if(NOT HW_ENABLE_D3D12_NATIVE_RASTER)
    target_link_libraries(homeworld PRIVATE OpenGL::GL)
endif()

if(HW_ENABLE_MOVIES)
    target_link_libraries(homeworld PRIVATE ${FFMPEG_LIBRARIES})
endif()

if(HW_ENABLE_D3D12_DIAGNOSTICS OR HW_ENABLE_D3D12_BACKEND)
    target_link_libraries(homeworld PRIVATE d3dcompiler dxguid comdlg32)
    if(HW_ENABLE_D3D12_BACKEND)
        # Streamline's import library supplies the D3D12/DXGI entry points and
        # installs NVIDIA's signed interposer before device creation.
        target_link_libraries(homeworld PRIVATE
            "${HW_STREAMLINE_INTERPOSER_LIBRARY}"
            "${HW_FIDELITYFX_LOADER_LIBRARY}"
            "${HW_XESS_LIBRARY}")
        target_include_directories(homeworld PRIVATE
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/api/include"
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/upscalers/include"
            "${HW_FIDELITYFX_ROOT}/Kits/FidelityFX/framegeneration/include"
            "${HW_XESS_ROOT}/include")
    else()
        target_link_libraries(homeworld PRIVATE d3d11 d3d12 dxgi)
    endif()
endif()

if(HW_ENABLE_NETWORK)
    if(TARGET SDL2_net::SDL2_net)
        target_link_libraries(homeworld PRIVATE SDL2_net::SDL2_net)
    elseif(TARGET SDL2_net::SDL2_net-static)
        target_link_libraries(homeworld PRIVATE SDL2_net::SDL2_net-static)
    else()
        message(FATAL_ERROR "SDL2_net was found, but it did not provide a supported CMake target.")
    endif()
endif()

if(TARGET SDL2::SDL2)
    add_custom_command(TARGET homeworld POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:homeworld>
        COMMENT "Copying the SDL2 runtime")
endif()

# The modern interface owns its presentation assets instead of depending on
# FEMan decorative LIFs embedded in Homeworld.big.  Copy them beside the EXE so
# SDL_GetBasePath() can resolve them regardless of the launch working folder.
add_custom_command(TARGET homeworld POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/UI"
        "$<TARGET_FILE_DIR:homeworld>/Data/UI"
    COMMENT "Copying Homeworld Modern interface artwork")

if(EXISTS "${HW_SOURCE_DIR}/out/asset-importer/HomeworldAssetImporter/HomeworldAssetImporter.exe")
    add_custom_command(TARGET homeworld POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${HW_SOURCE_DIR}/out/asset-importer/HomeworldAssetImporter"
            "$<TARGET_FILE_DIR:homeworld>/AssetImporter"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:homeworld>/tools"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${HW_SOURCE_DIR}/tools/Import-LocalAssets.ps1"
            "$<TARGET_FILE_DIR:homeworld>/tools/Import-LocalAssets.ps1"
        COMMENT "Copying standalone local asset importer")
endif()

add_custom_command(TARGET homeworld POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/Missions"
        "$<TARGET_FILE_DIR:homeworld>/Data/Missions"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/Planets"
        "$<TARGET_FILE_DIR:homeworld>/Data/Planets"
    COMMENT "Copying Homeworld Modern mission and planet artwork")

add_custom_command(TARGET homeworld POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/Cursors"
        "$<TARGET_FILE_DIR:homeworld>/Data/Cursors"
    COMMENT "Copying faithful high-definition Homeworld cursors")

add_custom_command(TARGET homeworld POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/Asteroids"
        "$<TARGET_FILE_DIR:homeworld>/Data/Asteroids"
    COMMENT "Copying subdivided Homeworld Modern asteroid artwork")

# Loose highest-detail ship textures.  Homeworld's resource layer gives these
# files precedence over the same paths inside Homeworld.big.  Only R1
# Mothership LOD0 is supplied because HW_MODERN_FORCE_LOD0 keeps that mesh at
# every distance in the modern build.
add_custom_command(TARGET homeworld POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${HW_SOURCE_DIR}/assets/R1"
        "$<TARGET_FILE_DIR:homeworld>/Data/R1"
    COMMENT "Copying Homeworld Modern LOD0 ship texture overrides")

if(HW_ENABLE_D3D12_BACKEND)
    foreach(_hw_streamline_file IN ITEMS
            sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll)
        add_custom_command(TARGET homeworld POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_streamline_file}"
                $<TARGET_FILE_DIR:homeworld>
            COMMENT "Copying NVIDIA Streamline runtime: ${_hw_streamline_file}")
    endforeach()
    foreach(_hw_streamline_rr_path IN LISTS HW_STREAMLINE_RR_RUNTIME_PATHS)
        get_filename_component(_hw_streamline_rr_file "${_hw_streamline_rr_path}" NAME)
        add_custom_command(TARGET homeworld POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_hw_streamline_rr_path}"
                $<TARGET_FILE_DIR:homeworld>
            COMMENT "Copying NVIDIA DLSS Ray Reconstruction runtime: ${_hw_streamline_rr_file}")
    endforeach()
    foreach(_hw_streamline_fg_file IN LISTS HW_STREAMLINE_FG_RUNTIME_NAMES)
        add_custom_command(TARGET homeworld POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${HW_STREAMLINE_RUNTIME_DIR}/${_hw_streamline_fg_file}"
                $<TARGET_FILE_DIR:homeworld>
            COMMENT "Copying NVIDIA frame-generation runtime: ${_hw_streamline_fg_file}")
    endforeach()
    foreach(_hw_vendor_runtime IN ITEMS
            "${HW_FIDELITYFX_ROOT}/bin/amd_fidelityfx_loader_dx12.dll"
            "${HW_FIDELITYFX_ROOT}/bin/amd_fidelityfx_upscaler_dx12.dll"
            "${HW_FIDELITYFX_FG_RUNTIME}"
            "${HW_XESS_ROOT}/bin/libxess.dll")
        add_custom_command(TARGET homeworld POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_hw_vendor_runtime}" $<TARGET_FILE_DIR:homeworld>
            COMMENT "Copying cross-vendor reconstruction runtime: ${_hw_vendor_runtime}")
    endforeach()
endif()

set_target_properties(homeworld PROPERTIES
    OUTPUT_NAME "HomeworldModern"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:homeworld>")

install(TARGETS homeworld RUNTIME DESTINATION .)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/UI/" DESTINATION Data/UI)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/Missions/" DESTINATION Data/Missions)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/Planets/" DESTINATION Data/Planets)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/Cursors/" DESTINATION Data/Cursors)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/Asteroids/" DESTINATION Data/Asteroids)
install(DIRECTORY "${HW_SOURCE_DIR}/assets/R1/" DESTINATION Data/R1)

if(BUILD_TESTING)
    add_executable(map_editor_object_inspector_tests "${HW_SOURCE_DIR}/tests/MapEditorObjectInspectorTests.c")
    add_test(NAME map_editor_object_inspector COMMAND map_editor_object_inspector_tests)
    add_executable(editor_instance_scale_tests "${HW_SOURCE_DIR}/tests/EditorInstanceScaleTests.c")
    add_test(NAME editor_instance_scale COMMAND editor_instance_scale_tests)
    add_executable(map_editor_catalog_tests "${HW_SOURCE_DIR}/tests/MapEditorCatalogTests.c")
    add_test(NAME map_editor_catalog COMMAND map_editor_catalog_tests)
    add_executable(backdrop_map_policy_tests "${HW_SOURCE_DIR}/tests/BackdropMapPolicyTests.c")
    add_test(NAME backdrop_map_policy COMMAND backdrop_map_policy_tests)
    add_executable(engine_trail_anchor_tests "${HW_SOURCE_DIR}/tests/EngineTrailAnchorTests.cpp")
    target_compile_features(engine_trail_anchor_tests PRIVATE cxx_std_17)
    add_test(NAME engine_trail_anchor COMMAND engine_trail_anchor_tests)
    add_executable(loading_progress_bridge_tests "${HW_SOURCE_DIR}/tests/LoadingProgressBridgeTests.cpp" "${HW_SOURCE_DIR}/src/Modern/MissionLoadProgress.cpp")
    target_compile_features(loading_progress_bridge_tests PRIVATE cxx_std_17)
    add_test(NAME loading_progress_all_modes COMMAND loading_progress_bridge_tests)
    add_executable(loading_artwork_tests "${HW_SOURCE_DIR}/tests/LoadingArtworkTests.c")
    add_test(NAME optional_loading_artwork COMMAND loading_artwork_tests)
    add_executable(motion_history_tests "${HW_SOURCE_DIR}/tests/MotionHistoryTests.cpp")
    target_compile_features(motion_history_tests PRIVATE cxx_std_17)
    add_test(NAME motion_history COMMAND motion_history_tests)
    add_executable(mission_load_progress_tests "${HW_SOURCE_DIR}/tests/MissionLoadProgressTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/MissionLoadProgress.cpp")
    target_compile_features(mission_load_progress_tests PRIVATE cxx_std_17)
    add_test(NAME mission_load_progress COMMAND mission_load_progress_tests)
    add_executable(startup_progress_tests "${HW_SOURCE_DIR}/tests/StartupProgressTests.cpp")
    target_link_libraries(startup_progress_tests PRIVATE hw_startup_progress)
    target_compile_features(startup_progress_tests PRIVATE cxx_std_17)
    add_test(NAME startup_progress COMMAND startup_progress_tests)
    add_executable(windows_splash_tests "${HW_SOURCE_DIR}/tests/WindowsSplashTests.cpp")
    target_include_directories(windows_splash_tests PRIVATE "${HW_SOURCE_DIR}/src/ThirdParty/stb")
    target_compile_features(windows_splash_tests PRIVATE cxx_std_17)
    target_compile_definitions(windows_splash_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(windows_splash_tests PRIVATE user32 gdi32)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/splash-tests")
    add_test(NAME windows_splash COMMAND windows_splash_tests "${CMAKE_CURRENT_BINARY_DIR}/splash-tests")
    add_executable(opaque_surface_policy_tests "${HW_SOURCE_DIR}/tests/OpaqueSurfacePolicyTests.cpp")
    target_include_directories(opaque_surface_policy_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(opaque_surface_policy_tests PRIVATE cxx_std_17)
    add_test(NAME opaque_surface_policy COMMAND opaque_surface_policy_tests)
    add_executable(frame_worker_tests tests/FrameWorkerTests.cpp)
    target_include_directories(frame_worker_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(frame_worker_tests PRIVATE cxx_std_17)
    target_compile_definitions(frame_worker_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_test(NAME concurrent_frame_workers COMMAND frame_worker_tests)
    set_tests_properties(concurrent_frame_workers PROPERTIES TIMEOUT 30)
    add_executable(benchmark_stats_tests "${HW_SOURCE_DIR}/tests/BenchmarkStatsTests.cpp")
    target_include_directories(benchmark_stats_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(benchmark_stats_tests PRIVATE cxx_std_17)
    add_test(NAME benchmark_stats COMMAND benchmark_stats_tests)
    if(WIN32)
        add_executable(fg_effect_guides_tests "${HW_SOURCE_DIR}/tests/FGEffectGuidesTests.cpp")
        target_compile_features(fg_effect_guides_tests PRIVATE cxx_std_17)
        target_compile_definitions(fg_effect_guides_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(fg_effect_guides_tests PRIVATE user32 d3d12 dxgi d3dcompiler)
        add_test(NAME fg_effect_guides COMMAND fg_effect_guides_tests)
        set_tests_properties(fg_effect_guides PROPERTIES TIMEOUT 60)
        add_executable(benchmark_capture_tests "${HW_SOURCE_DIR}/tests/BenchmarkCaptureTests.cpp" "${HW_SOURCE_DIR}/src/Modern/WindowsDiagnostics.cpp")
        target_include_directories(benchmark_capture_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
        target_compile_features(benchmark_capture_tests PRIVATE cxx_std_17)
        target_link_libraries(benchmark_capture_tests PRIVATE dxgi shell32 dbghelp)
        add_test(NAME benchmark_capture COMMAND benchmark_capture_tests -benchmark)
        add_test(NAME benchmark_disabled COMMAND benchmark_capture_tests -benchmark /noDiagnostics)
        add_executable(openhw_mod_tests tests/OpenHWModsTests.cpp src/Modern/OpenHWMods.cpp)
        target_compile_features(openhw_mod_tests PRIVATE cxx_std_17)
        target_compile_definitions(openhw_mod_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        add_test(NAME openhw_mods COMMAND openhw_mod_tests)
    endif()
    add_executable(dust_tile_bounds_tests "${HW_SOURCE_DIR}/tests/DustTileBoundsTests.cpp")
    target_include_directories(dust_tile_bounds_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(dust_tile_bounds_tests PRIVATE cxx_std_17)
    add_test(NAME dust_tile_bounds COMMAND dust_tile_bounds_tests)
    add_executable(replacement_texture_storage_tests "${HW_SOURCE_DIR}/tests/ReplacementTextureStorageTests.c")
    target_include_directories(replacement_texture_storage_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    add_test(NAME replacement_texture_storage COMMAND replacement_texture_storage_tests)
    add_executable(dust_shadow_policy_tests "${HW_SOURCE_DIR}/tests/DustShadowPolicyTests.cpp")
    target_include_directories(dust_shadow_policy_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(dust_shadow_policy_tests PRIVATE cxx_std_17)
    add_test(NAME dust_shadow_policy COMMAND dust_shadow_policy_tests)
    add_executable(texture_descriptor_tests "${HW_SOURCE_DIR}/tests/TextureDescriptorSlotsTests.cpp")
    target_include_directories(texture_descriptor_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(texture_descriptor_tests PRIVATE cxx_std_17)
    add_test(NAME texture_descriptor_lifetime COMMAND texture_descriptor_tests)
    add_executable(ui_scale_tests
        "${HW_SOURCE_DIR}/tests/UIScaleTests.c"
        "${HW_SOURCE_DIR}/src/SDL/uiscale.c")
    target_include_directories(ui_scale_tests PRIVATE
        "${HW_SOURCE_DIR}/src/Game"
        "${HW_SOURCE_DIR}/src/SDL")
    target_link_libraries(ui_scale_tests PRIVATE SDL2::SDL2)
    target_compile_features(ui_scale_tests PRIVATE c_std_11)
    add_test(NAME ui_scale_policy COMMAND ui_scale_tests)
    add_executable(native_cursor_tests "${HW_SOURCE_DIR}/tests/NativeCursorTests.c")
    target_include_directories(native_cursor_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_link_libraries(native_cursor_tests PRIVATE SDL2::SDL2)
    target_compile_features(native_cursor_tests PRIVATE c_std_11)
    add_test(NAME native_cursor COMMAND native_cursor_tests)
    add_executable(mouse_confinement_tests "${HW_SOURCE_DIR}/tests/MouseWindowConfinementTests.c")
    target_link_libraries(mouse_confinement_tests PRIVATE SDL2::SDL2)
    target_compile_definitions(mouse_confinement_tests PRIVATE SDL_MAIN_HANDLED)
    add_test(NAME mouse_window_confinement COMMAND mouse_confinement_tests)
    add_executable(pilot_collision_tests "${HW_SOURCE_DIR}/tests/PilotCollisionTests.c")
    add_test(NAME pilot_collision_policy COMMAND pilot_collision_tests)
    add_executable(save_layout_tests
        "${HW_SOURCE_DIR}/tests/SaveLayoutTests.c"
        "${HW_SOURCE_DIR}/src/Game/SaveGameLayout.c"
        "${HW_SOURCE_DIR}/src/Game/SaveGameLayoutClassic.c")
    get_target_property(_hw_save_includes homeworld INCLUDE_DIRECTORIES)
    get_target_property(_hw_save_defines homeworld COMPILE_DEFINITIONS)
    add_executable(effect_owner_tests "${HW_SOURCE_DIR}/tests/EffectOwnerTests.c")
    target_include_directories(effect_owner_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(effect_owner_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(effect_owner_tests PRIVATE SDL2::SDL2)
    target_compile_features(effect_owner_tests PRIVATE c_std_11)
    add_test(NAME effect_owner_types COMMAND effect_owner_tests)
    if(HW_ENABLE_D3D12_NATIVE_RASTER)
        add_executable(persistent_raster_tests
            "${HW_SOURCE_DIR}/tests/PersistentRasterTests.cpp"
            "${HW_SOURCE_DIR}/src/Game/ModernText.c"
            "${HW_SOURCE_DIR}/tests/SensorTextTestStubs.c"
            "${HW_SOURCE_DIR}/tests/ClassicShadeReference.c")
        target_include_directories(persistent_raster_tests PRIVATE ${_hw_save_includes})
        target_compile_definitions(persistent_raster_tests PRIVATE ${_hw_save_defines})
        target_compile_features(persistent_raster_tests PRIVATE c_std_11 cxx_std_17)
        target_link_libraries(persistent_raster_tests PRIVATE hw_gpu_memory hw_meshoptimizer hw_startup_progress
            Microsoft::DirectXTex SDL2::SDL2 d3d12 dxgi dxguid d3d11 d3dcompiler user32 gdi32 ole32)
        add_test(NAME persistent_raster_gpu_parity COMMAND persistent_raster_tests)
        set_tests_properties(persistent_raster_gpu_parity PROPERTIES TIMEOUT 60)
    endif()
    if(HW_ENABLE_D3D12_NATIVE_RASTER)
        add_executable(remastered_weapon_fx_runtime_tests
            "${HW_SOURCE_DIR}/tests/RemasteredWeaponFXRuntimeTests.c"
            "${HW_SOURCE_DIR}/src/Game/RemasteredWeaponFX.c")
        target_include_directories(remastered_weapon_fx_runtime_tests PRIVATE ${_hw_save_includes})
        target_compile_definitions(remastered_weapon_fx_runtime_tests PRIVATE ${_hw_save_defines})
        target_link_libraries(remastered_weapon_fx_runtime_tests PRIVATE SDL2::SDL2)
        target_compile_features(remastered_weapon_fx_runtime_tests PRIVATE c_std_11)
    endif()
    target_include_directories(save_layout_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(save_layout_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(save_layout_tests PRIVATE SDL2::SDL2)
    target_compile_features(save_layout_tests PRIVATE c_std_11)
    add_test(NAME save_layout_migration COMMAND save_layout_tests)
    add_executable(harvestable_dust_tests "${HW_SOURCE_DIR}/tests/HarvestableDustTests.c")
    add_executable(particle_field_tests "${HW_SOURCE_DIR}/tests/ParticleFieldTests.c" "${HW_SOURCE_DIR}/src/Game/Matrix.c" "${HW_SOURCE_DIR}/src/Game/Vector.c")
    target_include_directories(particle_field_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(particle_field_tests PRIVATE ${_hw_save_defines} SDL_MAIN_HANDLED)
    target_link_libraries(particle_field_tests PRIVATE SDL2::SDL2)
    target_compile_features(particle_field_tests PRIVATE c_std_11)
    add_test(NAME particle_field_clouds COMMAND particle_field_tests)
    if(WIN32)
        add_executable(particle_density_gpu_tests "${HW_SOURCE_DIR}/tests/ParticleDensityGpuTests.cpp")
        target_compile_features(particle_density_gpu_tests PRIVATE cxx_std_17)
        target_link_libraries(particle_density_gpu_tests PRIVATE d3d12 dxgi d3dcompiler)
    endif()
    target_include_directories(harvestable_dust_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(harvestable_dust_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(harvestable_dust_tests PRIVATE SDL2::SDL2)
    target_compile_features(harvestable_dust_tests PRIVATE c_std_11)
    add_test(NAME harvestable_dust_isolation COMMAND harvestable_dust_tests)
    add_executable(veterancy_toggle_tests "${HW_SOURCE_DIR}/tests/VeterancyToggleTests.c")
    target_include_directories(veterancy_toggle_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(veterancy_toggle_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(veterancy_toggle_tests PRIVATE SDL2::SDL2)
    target_compile_features(veterancy_toggle_tests PRIVATE c_std_11)
    add_test(NAME veterancy_toggle COMMAND veterancy_toggle_tests)
    add_executable(mission_start_launch_tests "${HW_SOURCE_DIR}/tests/MissionStartLaunchTests.c")
    target_include_directories(mission_start_launch_tests PRIVATE "${HW_SOURCE_DIR}/src/Game")
    target_compile_features(mission_start_launch_tests PRIVATE c_std_11)
    add_test(NAME mission_start_launch COMMAND mission_start_launch_tests)
    add_executable(resource_scaling_tests "${HW_SOURCE_DIR}/tests/ResourceScalingTests.c")
    target_include_directories(resource_scaling_tests PRIVATE "${HW_SOURCE_DIR}/src/Game")
    target_compile_features(resource_scaling_tests PRIVATE c_std_11)
    add_test(NAME sp_resource_scaling COMMAND resource_scaling_tests)
    add_executable(control_group_tab_tests "${HW_SOURCE_DIR}/tests/ControlGroupTabTests.c")
    target_include_directories(control_group_tab_tests PRIVATE "${HW_SOURCE_DIR}/src/Game")
    target_compile_features(control_group_tab_tests PRIVATE c_std_11)
    add_test(NAME control_group_tabs COMMAND control_group_tab_tests)
    add_executable(audio_frame_snapshot_tests tests/AudioFrameSnapshotTests.cpp)
    target_include_directories(audio_frame_snapshot_tests PRIVATE ${_hw_save_includes} "${HW_SOURCE_DIR}/src/Modern")
    target_compile_definitions(audio_frame_snapshot_tests PRIVATE ${_hw_save_defines} NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_features(audio_frame_snapshot_tests PRIVATE cxx_std_17)
    target_link_libraries(audio_frame_snapshot_tests PRIVATE SDL2::SDL2)
    add_test(NAME audio_frame_ownership_snapshot COMMAND audio_frame_snapshot_tests)
    add_executable(formation_spread_tests "${HW_SOURCE_DIR}/tests/FormationSpreadTests.c")
    target_include_directories(formation_spread_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(formation_spread_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(formation_spread_tests PRIVATE SDL2::SDL2)
    target_compile_features(formation_spread_tests PRIVATE c_std_11)
    add_test(NAME live_formation_spread COMMAND formation_spread_tests)
    add_executable(mesh_surface_cache_tests "${HW_SOURCE_DIR}/tests/MeshSurfaceCacheTests.c")
    target_include_directories(mesh_surface_cache_tests PRIVATE ${_hw_save_includes} "${HW_SOURCE_DIR}/src/Modern")
    target_compile_definitions(mesh_surface_cache_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(mesh_surface_cache_tests PRIVATE SDL2::SDL2)
    target_compile_features(mesh_surface_cache_tests PRIVATE c_std_11)
    add_test(NAME legacy_mesh_surface_cache COMMAND mesh_surface_cache_tests)
    add_executable(diagnostic_policy_tests tests/DiagnosticPolicyTests.cpp src/Modern/WindowsDiagnostics.cpp)
    target_include_directories(diagnostic_policy_tests PRIVATE src/Modern)
    target_compile_features(diagnostic_policy_tests PRIVATE cxx_std_17)
    target_compile_definitions(diagnostic_policy_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(diagnostic_policy_tests PRIVATE dbghelp shell32)
    add_test(NAME diagnostic_build_policy COMMAND diagnostic_policy_tests)
    if(HW_ENABLE_D3D12_NATIVE_RASTER)
    add_executable(raster_lighting_gpu_tests tests/RasterLightingGpuTests.cpp)
    target_include_directories(raster_lighting_gpu_tests PRIVATE src/Modern)
    target_compile_features(raster_lighting_gpu_tests PRIVATE cxx_std_17)
    target_compile_definitions(raster_lighting_gpu_tests PRIVATE HW_ENABLE_D3D12_NATIVE_RASTER NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(raster_lighting_gpu_tests PRIVATE SDL2::SDL2 Microsoft::DirectXTex d3d11 d3d12 dxgi d3dcompiler hw_gpu_memory hw_meshoptimizer hw_startup_progress)
    add_test(NAME raster_mission_lighting_gpu COMMAND raster_lighting_gpu_tests)
    set_tests_properties(raster_mission_lighting_gpu PROPERTIES TIMEOUT 60)
    endif()
    add_executable(launch_manager_tests "${HW_SOURCE_DIR}/tests/LaunchManagerTests.c")
    target_include_directories(launch_manager_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(launch_manager_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(launch_manager_tests PRIVATE SDL2::SDL2)
    target_compile_features(launch_manager_tests PRIVATE c_std_11)
    add_test(NAME launch_manager_lifecycle COMMAND launch_manager_tests)
    add_executable(ui_list_lifetime_tests "${HW_SOURCE_DIR}/tests/UIListLifetimeTests.c")
    target_include_directories(ui_list_lifetime_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(ui_list_lifetime_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(ui_list_lifetime_tests PRIVATE SDL2::SDL2)
    target_compile_features(ui_list_lifetime_tests PRIVATE c_std_11)
    add_test(NAME ui_list_lifetime COMMAND ui_list_lifetime_tests)
    add_executable(multiplayer_menu_layout_tests "${HW_SOURCE_DIR}/tests/MultiplayerMenuLayoutTests.c" "${HW_SOURCE_DIR}/src/SDL/uiscale.c")
    target_include_directories(multiplayer_menu_layout_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(multiplayer_menu_layout_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(multiplayer_menu_layout_tests PRIVATE SDL2::SDL2)
    target_compile_features(multiplayer_menu_layout_tests PRIVATE c_std_11)
    add_test(NAME multiplayer_menu_layout COMMAND multiplayer_menu_layout_tests)
    add_executable(shader_light_matrix_stack_tests "${HW_SOURCE_DIR}/tests/ShaderLightMatrixStackTests.c")
    target_include_directories(shader_light_matrix_stack_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(shader_light_matrix_stack_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(shader_light_matrix_stack_tests PRIVATE SDL2::SDL2)
    target_compile_features(shader_light_matrix_stack_tests PRIVATE c_std_11)
    add_test(NAME shader_light_matrix_stack COMMAND shader_light_matrix_stack_tests)
    add_executable(mission_station_docking_tests "${HW_SOURCE_DIR}/tests/MissionStationDockingTests.c")
    target_include_directories(mission_station_docking_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(mission_station_docking_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(mission_station_docking_tests PRIVATE SDL2::SDL2)
    target_compile_features(mission_station_docking_tests PRIVATE c_std_11)
    add_test(NAME mission_station_docking COMMAND mission_station_docking_tests)
    add_executable(dock_order_target_tests "${HW_SOURCE_DIR}/tests/DockOrderTargetTests.c")
    target_include_directories(dock_order_target_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(dock_order_target_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(dock_order_target_tests PRIVATE SDL2::SDL2)
    target_compile_features(dock_order_target_tests PRIVATE c_std_11)
    add_test(NAME dock_order_target COMMAND dock_order_target_tests)
    add_executable(unlimited_resource_units_tests "${HW_SOURCE_DIR}/tests/UnlimitedResourceUnitsTests.c")
    target_include_directories(unlimited_resource_units_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(unlimited_resource_units_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(unlimited_resource_units_tests PRIVATE SDL2::SDL2)
    target_compile_features(unlimited_resource_units_tests PRIVATE c_std_11)
    add_test(NAME unlimited_resource_units COMMAND unlimited_resource_units_tests)
    add_executable(ship_pilot_tests "${HW_SOURCE_DIR}/tests/ShipPilotTests.c")
    target_include_directories(ship_pilot_tests PRIVATE ${_hw_save_includes} "${HW_SOURCE_DIR}/src/Ships")
    target_compile_definitions(ship_pilot_tests PRIVATE ${_hw_save_defines} SDL_MAIN_HANDLED)
    target_link_libraries(ship_pilot_tests PRIVATE SDL2::SDL2)
    target_compile_features(ship_pilot_tests PRIVATE c_std_11)
    add_test(NAME ship_pilot_controls COMMAND ship_pilot_tests)
    add_executable(audio_bus_tests "${HW_SOURCE_DIR}/tests/AudioBusTests.c")
    target_include_directories(audio_bus_tests PRIVATE "${HW_SOURCE_DIR}/src/SDL")
    target_compile_features(audio_bus_tests PRIVATE c_std_11)
    add_test(NAME audio_bus_gain COMMAND audio_bus_tests)
    add_executable(pcm_stream_tests "${HW_SOURCE_DIR}/tests/PcmStreamTests.c")
    target_include_directories(pcm_stream_tests PRIVATE "${HW_SOURCE_DIR}/src/SDL" "${HW_SOURCE_DIR}/src/Game")
    target_compile_features(pcm_stream_tests PRIVATE c_std_11)
    add_test(NAME pcm_stream COMMAND pcm_stream_tests)
    add_executable(remastered_pcm_stream_tests "${HW_SOURCE_DIR}/tests/RemasteredPcmStreamTests.c"
        "${HW_SOURCE_DIR}/src/SDL/RemasteredPcmStream.c" "${HW_SOURCE_DIR}/src/SDL/RemasteredAudioDecode.c")
    target_include_directories(remastered_pcm_stream_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(remastered_pcm_stream_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(remastered_pcm_stream_tests PRIVATE SDL2::SDL2)
    target_compile_features(remastered_pcm_stream_tests PRIVATE c_std_11)
    add_test(NAME remastered_pcm_stream COMMAND remastered_pcm_stream_tests)
    add_executable(remastered_audio_decode_tests "${HW_SOURCE_DIR}/tests/RemasteredAudioDecodeTests.cpp"
        "${HW_SOURCE_DIR}/src/SDL/RemasteredAudioDecode.c" "${HW_SOURCE_DIR}/src/SDL/fquant.c"
        "${HW_SOURCE_DIR}/src/SDL/fqcodec.c" "${HW_SOURCE_DIR}/src/SDL/dct.c" "${HW_SOURCE_DIR}/src/SDL/mixfft.c")
    target_include_directories(remastered_audio_decode_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(remastered_audio_decode_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(remastered_audio_decode_tests PRIVATE SDL2::SDL2)
    target_compile_features(remastered_audio_decode_tests PRIVATE c_std_11 cxx_std_17)
    add_test(NAME remastered_audio_decode COMMAND remastered_audio_decode_tests)
    add_executable(presentation_events_tests "${HW_SOURCE_DIR}/tests/PresentationEventsTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp")
    target_include_directories(presentation_events_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(presentation_events_tests PRIVATE cxx_std_17)
    add_test(NAME presentation_events COMMAND presentation_events_tests)
    add_executable(weapon_presentation_tests "${HW_SOURCE_DIR}/tests/WeaponPresentationTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp")
    target_include_directories(weapon_presentation_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(weapon_presentation_tests PRIVATE cxx_std_17)
    add_test(NAME weapon_presentation COMMAND weapon_presentation_tests)
    add_executable(sound_bank_tests "${HW_SOURCE_DIR}/tests/SoundBankTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundBank.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundPlayback.cpp")
    target_include_directories(sound_bank_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(sound_bank_tests PRIVATE cxx_std_17)
    add_test(NAME sound_bank COMMAND sound_bank_tests)
    add_executable(fx_graph_tests "${HW_SOURCE_DIR}/tests/FxGraphTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxGraph.cpp")
    target_include_directories(fx_graph_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(fx_graph_tests PRIVATE cxx_std_17)
    add_test(NAME fx_graph COMMAND fx_graph_tests)
    add_executable(fx_playback_tests "${HW_SOURCE_DIR}/tests/FxPlaybackTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxGraph.cpp" "${HW_SOURCE_DIR}/src/Modern/RemasteredFxPlayback.cpp")
    target_include_directories(fx_playback_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(fx_playback_tests PRIVATE cxx_std_17)
    add_test(NAME fx_playback COMMAND fx_playback_tests)
    add_executable(fx_resources_tests "${HW_SOURCE_DIR}/tests/FxResourcesTests.cpp" "${HW_SOURCE_DIR}/src/Modern/RemasteredFxResources.cpp")
    target_include_directories(fx_resources_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(fx_resources_tests PRIVATE cxx_std_17)
    add_test(NAME fx_resources COMMAND fx_resources_tests)
    add_executable(fx_renderer_tests "${HW_SOURCE_DIR}/tests/FxRendererTests.c"
        "${HW_SOURCE_DIR}/src/Game/RemasteredFxRenderer.c"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxGraph.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxResources.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp")
    target_include_directories(fx_renderer_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(fx_renderer_tests PRIVATE ${_hw_save_defines})
    target_compile_features(fx_renderer_tests PRIVATE c_std_11 cxx_std_17)
    target_link_libraries(fx_renderer_tests PRIVATE SDL2::SDL2)
    add_test(NAME fx_renderer COMMAND fx_renderer_tests)
    add_executable(presentation_runtime_tests
        "${HW_SOURCE_DIR}/tests/RemasteredPresentationRuntimeTests.c"
        "${HW_SOURCE_DIR}/src/Game/RemasteredPresentation.c"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundBank.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSoundPlayback.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxGraph.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxPlayback.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredFxResources.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp"
        "${HW_SOURCE_DIR}/src/SDL/RemasteredPcmStream.c"
        "${HW_SOURCE_DIR}/src/SDL/RemasteredAudioDecode.c")
    target_include_directories(presentation_runtime_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(presentation_runtime_tests PRIVATE ${_hw_save_defines})
    target_compile_features(presentation_runtime_tests PRIVATE c_std_11 cxx_std_17)
    target_link_libraries(presentation_runtime_tests PRIVATE SDL2::SDL2)
    add_test(NAME presentation_runtime COMMAND presentation_runtime_tests)
    add_executable(obig_presentation_tests "${HW_SOURCE_DIR}/tests/ObigPresentationTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/OpenHWArchive.cpp")
    target_include_directories(obig_presentation_tests PRIVATE "${HW_SOURCE_DIR}/src/Game")
    target_compile_definitions(obig_presentation_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_features(obig_presentation_tests PRIVATE cxx_std_17)
    add_test(NAME obig_presentation COMMAND obig_presentation_tests)
    add_executable(classic_gun_rest_pose_tests "${HW_SOURCE_DIR}/tests/ClassicGunRestPoseTests.cpp")
    target_compile_features(classic_gun_rest_pose_tests PRIVATE cxx_std_17)
    add_test(NAME classic_gun_rest_pose COMMAND classic_gun_rest_pose_tests)
    add_executable(engine_fade_tests "${HW_SOURCE_DIR}/tests/EngineFadeTests.cpp")
    add_test(NAME engine_fade COMMAND engine_fade_tests)
    add_executable(memory_growth_tests "${HW_SOURCE_DIR}/tests/MemoryGrowthTests.c"
        "${HW_SOURCE_DIR}/src/Game/Memory.c" "${HW_SOURCE_DIR}/src/Game/LinkedList.c"
        "${HW_SOURCE_DIR}/src/ThirdParty/CRC/CRC32.c")
    target_include_directories(memory_growth_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(memory_growth_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(memory_growth_tests PRIVATE SDL2::SDL2)
    add_test(NAME memory_growth COMMAND memory_growth_tests)
    add_executable(native_ship_hod_tests
        "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp"
        "${HW_SOURCE_DIR}/tests/NativeShipHodTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredVisualAsset.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredVisualPackage.cpp"
        "${HW_SOURCE_DIR}/src/Game/NativeVisualRegistry.c"
        "${HW_SOURCE_DIR}/src/Game/NativeVisualEvents.c")
    target_include_directories(native_ship_hod_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(native_ship_hod_tests PRIVATE cxx_std_17)
    add_test(NAME native_interceptor_hod COMMAND native_ship_hod_tests)
    if(WIN32 AND HW_ENABLE_D3D12_BACKEND)
        # Actual integration against CPU mocks; no Streamline DLL/device/GPU.
        add_executable(streamline_failure_tests "${HW_SOURCE_DIR}/tests/StreamlineFailureTests.cpp")
        target_include_directories(streamline_failure_tests PRIVATE
            "${HW_SOURCE_DIR}/src/Modern" "${HW_STREAMLINE_INCLUDE_DIR}")
        target_compile_definitions(streamline_failure_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_features(streamline_failure_tests PRIVATE cxx_std_17)
        add_test(NAME streamline_failure_bounds COMMAND streamline_failure_tests)
    endif()

    add_executable(paged_array_tests "${HW_SOURCE_DIR}/tests/PagedArrayTests.cpp")
    target_include_directories(paged_array_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(paged_array_tests PRIVATE cxx_std_17)
    add_test(NAME paged_surface_texels COMMAND paged_array_tests)
    add_executable(material_texture_residency_tests "${HW_SOURCE_DIR}/tests/MaterialTextureResidencyTests.cpp")
    target_include_directories(material_texture_residency_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(material_texture_residency_tests PRIVATE cxx_std_17)
    add_test(NAME material_texture_residency COMMAND material_texture_residency_tests)
    if(HW_ENABLE_D3D12_BACKEND)
        add_executable(gpu_memory_allocator_tests "${HW_SOURCE_DIR}/tests/GpuMemoryAllocatorTests.cpp")
        target_compile_definitions(gpu_memory_allocator_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(gpu_memory_allocator_tests PRIVATE hw_gpu_memory)
        add_test(NAME gpu_memory_allocator COMMAND gpu_memory_allocator_tests)
        set(HW_SAMPLER_FEEDBACK_TEST_SHADER "${HW_GENERATED_DIR}/Modern/SamplerFeedbackTest.cso")
        add_custom_command(OUTPUT "${HW_SAMPLER_FEEDBACK_TEST_SHADER}"
            COMMAND "${HW_DXC_EXECUTABLE}" -T cs_6_5 -E TestCS -HV 2021 -O3
                -Fo "${HW_SAMPLER_FEEDBACK_TEST_SHADER}"
                "${HW_SOURCE_DIR}/tests/SamplerFeedbackTest.hlsl"
            DEPENDS "${HW_SOURCE_DIR}/tests/SamplerFeedbackTest.hlsl" VERBATIM)
        add_executable(sampler_feedback_gpu_tests
            "${HW_SOURCE_DIR}/tests/SamplerFeedbackGpuTests.cpp"
            "${HW_SAMPLER_FEEDBACK_TEST_SHADER}")
        target_compile_definitions(sampler_feedback_gpu_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(sampler_feedback_gpu_tests PRIVATE hw_material_streaming)
        add_test(NAME sampler_feedback_gpu COMMAND sampler_feedback_gpu_tests "${HW_SAMPLER_FEEDBACK_TEST_SHADER}")
    endif()
    if(WIN32)
        add_executable(prepared_material_cache_tests "${HW_SOURCE_DIR}/tests/PreparedMaterialCacheTests.cpp")
        target_include_directories(prepared_material_cache_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
        target_compile_features(prepared_material_cache_tests PRIVATE cxx_std_17)
        target_compile_definitions(prepared_material_cache_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(prepared_material_cache_tests PRIVATE Microsoft::DirectXTex)
        add_test(NAME prepared_material_cache COMMAND prepared_material_cache_tests)
        add_executable(lossless_texel_pages_tests "${HW_SOURCE_DIR}/tests/LosslessTexelPagesTests.cpp")
        target_include_directories(lossless_texel_pages_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
        target_compile_features(lossless_texel_pages_tests PRIVATE cxx_std_17)
        target_compile_definitions(lossless_texel_pages_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(lossless_texel_pages_tests PRIVATE cabinet Microsoft::DirectXTex)
        add_test(NAME lossless_material_texel_pages COMMAND lossless_texel_pages_tests)
    endif()
    # The white-box ownership test includes RemasteredVisualAsset.cpp itself.
    add_executable(shared_hod_scene_tests
        "${HW_SOURCE_DIR}/tests/SharedHodSceneTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredPresentationEvents.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredWeaponPresentation.cpp")
    target_include_directories(shared_hod_scene_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(shared_hod_scene_tests PRIVATE cxx_std_17)
    find_package(Threads REQUIRED)
    target_link_libraries(shared_hod_scene_tests PRIVATE Threads::Threads)
    add_test(NAME shared_hod_scene_ownership COMMAND shared_hod_scene_tests)

    add_executable(native_ship_mad_tests
        "${HW_SOURCE_DIR}/tests/NativeShipMadTests.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp")
    target_include_directories(native_ship_mad_tests PRIVATE "${HW_SOURCE_DIR}/src/Modern")
    target_compile_features(native_ship_mad_tests PRIVATE cxx_std_17)
    add_test(NAME native_ship_mad COMMAND native_ship_mad_tests)
    # Local asset-staging API only. Never copied into the deployed game; the
    # game links the same reader directly and has no dependency on this DLL.
    add_library(remastered_asset_reader SHARED EXCLUDE_FROM_ALL
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp")
    target_compile_features(remastered_asset_reader PRIVATE cxx_std_17)
    set_target_properties(remastered_asset_reader PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_executable(OpenHWAssetCheck tools/openhw_asset_check.cpp src/Modern/OpenHWMods.cpp
        src/Modern/RemasteredPresentationEvents.cpp
        src/Modern/RemasteredWeaponPresentation.cpp
        src/Modern/RemasteredFxGraph.cpp
        src/Modern/RemasteredFxResources.cpp
        src/Modern/RemasteredSoundBank.cpp
        src/Modern/OpenHWArchive.cpp src/Modern/RemasteredShipHod.cpp
        src/Modern/RemasteredShipMad.cpp src/Modern/RemasteredSubsystem.cpp
        src/Modern/RemasteredVisualAsset.cpp src/Modern/RemasteredVisualPackage.cpp)
    target_compile_features(OpenHWAssetCheck PRIVATE cxx_std_17)
    target_compile_definitions(OpenHWAssetCheck PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    add_executable(remastered_mesh_bridge_tests
        "${HW_SOURCE_DIR}/tests/RemasteredMeshBridgeTests.cpp"
        "${HW_SOURCE_DIR}/src/Game/RemasteredMeshBridge.c"
        "${HW_SOURCE_DIR}/src/ThirdParty/CRC/CRC32.c"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipHod.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredShipMad.cpp"
        "${HW_SOURCE_DIR}/src/Modern/RemasteredSubsystem.cpp")
    target_include_directories(remastered_mesh_bridge_tests PRIVATE ${_hw_save_includes})
    target_compile_definitions(remastered_mesh_bridge_tests PRIVATE ${_hw_save_defines})
    target_link_libraries(remastered_mesh_bridge_tests PRIVATE SDL2::SDL2)
    target_compile_features(remastered_mesh_bridge_tests PRIVATE c_std_11 cxx_std_17)
    add_test(NAME remastered_mesh_bridge COMMAND remastered_mesh_bridge_tests)
endif()
