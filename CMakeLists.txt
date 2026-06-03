cmake_minimum_required(VERSION 3.16)

project(godot-avbridge VERSION 0.1.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_GENERATOR_PLATFORM MATCHES "arm64|aarch64|ARM64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ARCH "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x86_64")
else()
    set(ARCH "x86_32")
endif()

# godot-cpp
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/godot-cpp")

# libavbridge — built as a static library and linked directly into the
# extension, so a single shared object ships in the addon. The gstreamer/ffmpeg
# backends are still loaded at runtime via dlopen (not linked here).
set(AVB_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(AVB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(AVB_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/libavbridge" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE INCLUDES "src/*.hpp")
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(GodotAVBridge SHARED ${SOURCES} ${INCLUDES})

set_target_properties(GodotAVBridge PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "godot-avbridge.$<IF:$<PLATFORM_ID:Windows>,windows,$<IF:$<PLATFORM_ID:Darwin>,macos,linux>>.$<IF:$<CONFIG:Debug>,debug,release>.${ARCH}"
)

target_include_directories(GodotAVBridge PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/lib/godot-cpp/gdextension
    ${PROJECT_SOURCE_DIR}/lib/godot-cpp/include
    ${PROJECT_SOURCE_DIR}/lib/godot-cpp/gen/include
    ${PROJECT_SOURCE_DIR}/lib/libavbridge/include
)

target_link_libraries(GodotAVBridge PRIVATE godot-cpp avbridge)

set_target_properties(GodotAVBridge PROPERTIES
    SKIP_RPATH TRUE
    BUILD_RPATH_USE_ORIGIN TRUE
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE)
if(APPLE)
    set_target_properties(GodotAVBridge PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(GodotAVBridge PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

install(TARGETS GodotAVBridge
    RUNTIME DESTINATION "${PROJECT_SOURCE_DIR}/demo/addons/godot-avbridge/bin"
    LIBRARY DESTINATION "${PROJECT_SOURCE_DIR}/demo/addons/godot-avbridge/bin"
)
