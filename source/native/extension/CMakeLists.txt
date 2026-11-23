cmake_minimum_required(VERSION 3.25)
project(pes6_native_extension LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PES6_USE_SYSTEM_STL_HELPERS "Use the STL byte-search helpers provided by recent MSVC runtimes" OFF)
find_package(rexglue 0.10.0 REQUIRED CONFIG)
add_subdirectory(thirdparty/minhook)
add_library(pes6-native-extension SHARED extension.cpp overlay.cpp presentation.cpp stl_compat.cpp generated/patched.cpp generated/forwards.cpp
 ../src/runtime_compat.cpp ../src/player_editor_runtime.cpp ../src/native_player_editor.cpp
 ../src/native_player_preview.cpp ../src/setpiece_gameplay.cpp ../src/game_camera.cpp ../src/game_cloth.cpp ../src/game_cloth_resources.cpp ../src/game_net_sound.cpp ../src/game_weather.cpp ../src/game_weather_menu.cpp ../src/game_stadium.cpp ../src/game_ball.cpp)
target_include_directories(pes6-native-extension PRIVATE ../generated/default)
if(PES6_USE_SYSTEM_STL_HELPERS)
    target_compile_definitions(pes6-native-extension PRIVATE PES6_USE_SYSTEM_STL_HELPERS=1)
endif()
target_link_libraries(pes6-native-extension PRIVATE rex::runtime minhook winmm)
target_compile_options(pes6-native-extension PRIVATE -mavx2 -ffp-contract=off)
set_source_files_properties(generated/patched.cpp PROPERTIES COMPILE_OPTIONS "-fasync-exceptions")
set_source_files_properties(../src/runtime_compat.cpp PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/import-routing.h")

