# pes6 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(pes6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PES6_USE_SYSTEM_STL_HELPERS "Use the STL byte-search helpers provided by recent MSVC runtimes" OFF)

include(generated/rexglue.cmake)

# Use the executable exported by the selected SDK package. Overriding it with
# a path from another checkout breaks builds when the project is moved.

# Sources
set(PES6_SOURCES
    src/main.cpp
    src/runtime_compat.cpp
    src/player_editor.cpp
    src/player_editor_runtime.cpp
    src/native_player_editor.cpp
    src/native_player_preview.cpp
    src/setpiece_gameplay.cpp
    src/game_camera.cpp
    src/game_cloth.cpp
    src/game_cloth_resources.cpp
    src/game_net_sound.cpp
    src/game_weather.cpp
    src/game_weather_menu.cpp
    src/game_stadium.cpp
    src/game_ball.cpp
)

if(WIN32)
    add_executable(pes6 WIN32 ${PES6_SOURCES})
else()
    add_executable(pes6 ${PES6_SOURCES})
endif()

rexglue_setup_target(pes6 GPU_PLUGINS xenos)
if(PES6_USE_SYSTEM_STL_HELPERS)
    target_compile_definitions(pes6 PRIVATE PES6_USE_SYSTEM_STL_HELPERS=1)
endif()

add_custom_command(TARGET pes6_codegen POST_BUILD
    COMMAND node "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/patch-generated.cjs"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
    target_compile_definitions(pes6 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(pes6_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(pes6 PRIVATE winmm)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(pes6 PRIVATE "LINKER:/OPT:REF" "LINKER:/OPT:ICF")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(PES6_BASE_ISA_FLAGS -mavx2 -mbmi -mbmi2 -mlzcnt -mpopcnt -mmovbe -fno-math-errno -fno-trapping-math)
    target_compile_options(pes6 PRIVATE ${PES6_BASE_ISA_FLAGS})
    target_compile_options(pes6_recomp PRIVATE ${PES6_BASE_ISA_FLAGS} -ffp-contract=off)
else()
    target_compile_options(pes6_recomp PRIVATE -mavx2 -ffp-contract=off)
endif()

