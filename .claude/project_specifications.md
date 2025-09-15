# CCABN VideoStream GDExtension Project Specifications

## Project Overview
This GDExtension provides a VideoStreamReceiver node for receiving and displaying video streams over HTTP, specifically designed for ETVR (EyeTrackVR) integration and AI model processing.


## Requirements

### Core Functionality
- **Video Stream Reception**: Receive HTTP video streams from local network devices
- **Display**: 240x240 pixel GUI element displaying grayscale video feed
- **Fallback Display**: Dark gray square when no feed or errors occur
- **Protocol**: HTTP protocol with IP address and port configuration
- **Format**: ETVR uses MJPEG over HTTP format for video streaming

### Node Specifications
- **Base Class**: Control (GUI element)
- **Size**: Fixed 240x240 pixels
- **Video Input**: 240x240p grayscale (black and white)
- **Inspector Integration**: All variables accessible through Godot's Inspector
- **Cross-Node Access**: Video feed and status accessible by other nodes

### Variables & Properties
1. **IP Address** (String): Target device IP address (e.g., "192.168.1.100")
2. **Port** (int): Target device port (default: 8080)
3. **Connection Status** (String, read-only): Current connection state
   - "No Address" - No IP entered
   - "Connecting" - Attempting connection
   - "Connected" - Successfully streaming
   - "Request Failed" - Network error
   - "HTTP Error XXX" - HTTP response error
   - "Invalid Image Format" - Unexpected format
   - "Disconnected" - Stream stopped
4. **Brightness Level** (float, read-only): -1 to 1 scale
   - -1: Too dark/underexposed (black)
   - 0: Optimal brightness
   - 1: Too bright/overexposed (white)
5. **Video Texture** (ImageTexture): Accessible video feed for other nodes

### Technical Implementation
- **HTTP Endpoint**: `/snapshot` for single-frame requests (not `/stream`)
- **Refresh Rate**: ~30 FPS using Timer with 0.033s interval
- **Image Processing**: JPEG decode → resize to 240x240 → convert to RGB8
- **Brightness Detection**: Average pixel brightness analysis with optimal range detection
- **Error Handling**: Robust connection state management with fallback display

### Use Case
This node enables:
1. **Video Display**: Real-time video feed display in Godot UI
2. **AI Processing**: Video feed input for facial expression AI models
3. **Brightness Monitoring**: Automatic lighting condition assessment
4. **Status Monitoring**: Connection and stream health information

The end goal is facial expression detection from video input, mapping expressions to 3D face models, with automatic brightness adjustment recommendations.

## Build System
- **CMake** build system with godot-cpp integration
- **C++17** standard
- **Cross-platform** support (Linux, Windows, macOS)
- **GDExtension** format for Godot 4.1+

## Files Structure
```
src/
├── register_types.cpp/h     # GDExtension registration
├── simple_logger.cpp/h     # Basic logging node
└── video_stream_receiver.cpp/h  # Main video streaming node
```