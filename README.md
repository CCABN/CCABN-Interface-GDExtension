# CCABN-VideoStream-GDExtension

GDExtension for streaming video into Godot as UI element. This is an example repository with everything needed to start developing a video streaming GDExtension for Godot 4.3+.

## Project Structure

```
CCABN-VideoStream-GDExtension/
├── src/                              # C++ source files
│   ├── video_stream.h               # Main VideoStream class header
│   ├── video_stream.cpp             # Main VideoStream class implementation
│   ├── register_types.h             # Registration header
│   └── register_types.cpp           # GDExtension registration and initialization
├── extern/
│   └── godot-cpp/                   # Godot C++ bindings (submodule)
├── bin/                             # Compiled extension libraries (created during build)
├── CMakeLists.txt                   # CMake build configuration
├── ccabn_videostream.gdextension    # Extension configuration file
└── .gitignore                       # Git ignore file

## Features

- **VideoStream Class**: A Control-based node that can be used in Godot scenes
- **Stream URL Configuration**: Set and get video stream URLs
- **Stream Control**: Start and stop video streaming
- **Cross-Platform**: Builds for Windows, macOS, and Linux
- **CMake Build System**: Modern CMake configuration for easy building

## Prerequisites

- CMake 3.22 or later
- C++17 compatible compiler
- Git (for submodules)

### Platform-specific requirements:
- **Windows**: Visual Studio 2019 or later, or MinGW-w64
- **macOS**: Xcode command line tools
- **Linux**: GCC 7+ or Clang 6+

## Building

1. **Clone the repository with submodules:**
   ```bash
   git clone --recursive https://github.com/yourusername/CCABN-VideoStream-GDExtension.git
   cd CCABN-VideoStream-GDExtension
   ```

   If you already cloned without `--recursive`, initialize submodules:
   ```bash
   git submodule update --init --recursive
   ```

2. **Create build directory:**
   ```bash
   mkdir build
   cd build
   ```

3. **Configure and build:**
   ```bash
   # Debug build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build .
   
   # Release build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build .
   ```

4. **The compiled library will be in the `bin/` directory**

## Using in Godot

1. Copy the entire extension folder to your Godot project
2. The `ccabn_videostream.gdextension` file should be in your project root or addons folder
3. The extension will be automatically loaded by Godot
4. You can now use the `VideoStream` node in your scenes

### Example usage in GDScript:

```gdscript
extends Control

@onready var video_stream = $VideoStream

func _ready():
    video_stream.stream_url = "rtmp://example.com/stream"
    video_stream.start_stream()

func _on_stop_button_pressed():
    video_stream.stop_stream()
```

## Development Notes

### Current Implementation Status

This is a **template/example** implementation. The VideoStream class currently includes:

- ✅ Basic node structure and Godot integration
- ✅ Property binding for stream URL
- ✅ Start/stop stream methods
- ❌ **Actual video decoding and streaming** (TODO)
- ❌ **Frame rendering to texture** (TODO)
- ❌ **Audio handling** (TODO)

### Next Steps for Implementation

To make this a functional video streaming extension, you'll need to:

1. **Add video decoding library** (e.g., FFmpeg, GStreamer)
2. **Implement frame decoding** in `VideoStream::_process()`
3. **Convert frames to Godot Texture2D**
4. **Handle audio streams**
5. **Add error handling and reconnection logic**
6. **Optimize performance** for real-time streaming

### Integration with CCABN-Software

This extension is designed to be used with the main CCABN-Software repository. You can:

1. **Git Submodule**: Add this as a submodule to CCABN-Software
2. **Direct Integration**: Copy the built extension into your main project
3. **Package Manager**: Use as a Godot addon package

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

[Add your license here]
