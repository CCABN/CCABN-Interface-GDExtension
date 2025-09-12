cmake_minimum_required(VERSION 3.22)

project(ccabn-videostream-gdextension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add godot-cpp dependency
add_subdirectory(extern/godot-cpp)

# Create our library
file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(ccabn-videostream SHARED ${SOURCES})

# Link godot-cpp
target_link_libraries(ccabn-videostream PRIVATE godot-cpp)

# Include directories
target_include_directories(ccabn-videostream PRIVATE src/)

# Set output directory
set_target_properties(ccabn-videostream PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

# Platform-specific settings
if(WIN32)
    set_target_properties(ccabn-videostream PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(ccabn-videostream PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(ccabn-videostream PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Set output name based on platform and configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE_SUFFIX ".debug")
else()
    set(BUILD_TYPE_SUFFIX ".release")
endif()

if(WIN32)
    set_target_properties(ccabn-videostream PROPERTIES
        OUTPUT_NAME "ccabn_videostream.windows${BUILD_TYPE_SUFFIX}.x86_64"
    )
elseif(APPLE)
    set_target_properties(ccabn-videostream PROPERTIES
        OUTPUT_NAME "ccabn_videostream.macos${BUILD_TYPE_SUFFIX}.universal"
    )
else()
    set_target_properties(ccabn-videostream PROPERTIES
        OUTPUT_NAME "ccabn_videostream.linux${BUILD_TYPE_SUFFIX}.x86_64"
    )
endif()