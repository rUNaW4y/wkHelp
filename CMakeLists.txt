cmake_minimum_required(VERSION 3.20)
project(wkHelp LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(WORM_TRACKER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wkWormsTracker")
add_library(hacklib STATIC
    "${WORM_TRACKER_ROOT}/lib/hacklib/ExeFile.cpp"
    "${WORM_TRACKER_ROOT}/lib/hacklib/ExeFile_WIN32.cpp"
    "${WORM_TRACKER_ROOT}/lib/hacklib/PatternScanner.cpp"
    "${WORM_TRACKER_ROOT}/lib/hacklib/Memory_WIN32.cpp")
target_include_directories(hacklib PUBLIC "${WORM_TRACKER_ROOT}/lib/hacklib")
link_directories("${WORM_TRACKER_ROOT}/lib")
add_library(wkHelp SHARED
    src/main.cpp src/GameState.cpp src/GameState.h src/Log.cpp src/Log.h
    src/scheme/SchemeInfo.cpp src/scheme/SchemeInfo.h
    src/scheme/Config.cpp src/scheme/Config.h
    src/scheme/Hooks.cpp src/scheme/Hooks.h
    src/scheme/Utils.cpp src/scheme/Utils.h
    src/scheme/Diagnostics.cpp src/scheme/Diagnostics.h
    src/scheme/Debugf.h resources.rc)
target_include_directories(wkHelp PRIVATE "${WORM_TRACKER_ROOT}/include" "${WORM_TRACKER_ROOT}/lib/hacklib" "${CMAKE_CURRENT_SOURCE_DIR}/src/scheme")
target_compile_definitions(wkHelp PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(wkHelp PRIVATE user32 gdi32 gdiplus ole32 version hacklib capstone PolyHook_2)
set_target_properties(wkHelp PROPERTIES OUTPUT_NAME wkHelp)
