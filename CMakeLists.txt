cmake_minimum_required(VERSION 3.20)
project(ccabn_interface)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add godot-cpp dependency
add_subdirectory(extern/godot-cpp)

# Add the source files
set(SOURCES
    src/register_types.cpp
    src/simple_logger.cpp
    src/camera_stream_receiver.cpp
)

# Create the shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Link godot-cpp
target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src/)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)