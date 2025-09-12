#include "video_stream.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void VideoStream::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_stream_url", "url"), &VideoStream::set_stream_url);
    ClassDB::bind_method(D_METHOD("get_stream_url"), &VideoStream::get_stream_url);
    ClassDB::bind_method(D_METHOD("start_stream"), &VideoStream::start_stream);
    ClassDB::bind_method(D_METHOD("stop_stream"), &VideoStream::stop_stream);
    ClassDB::bind_method(D_METHOD("is_stream_active"), &VideoStream::is_stream_active);

    ClassDB::add_property("VideoStream", PropertyInfo(Variant::STRING, "stream_url"), "set_stream_url", "get_stream_url");
}

VideoStream::VideoStream() {
    stream_url = "";
    is_streaming = false;
}

VideoStream::~VideoStream() {
    if (is_streaming) {
        stop_stream();
    }
}

void VideoStream::set_stream_url(const String &p_url) {
    stream_url = p_url;
}

String VideoStream::get_stream_url() const {
    return stream_url;
}

void VideoStream::start_stream() {
    if (stream_url.is_empty()) {
        UtilityFunctions::print("Error: Stream URL is empty");
        return;
    }
    
    is_streaming = true;
    UtilityFunctions::print("Starting video stream: ", stream_url);
    
    // TODO: Implement actual video streaming logic here
    // This would involve:
    // 1. Connecting to the video stream source
    // 2. Decoding video frames
    // 3. Converting frames to Godot Texture2D
    // 4. Updating the UI control to display the frame
}

void VideoStream::stop_stream() {
    is_streaming = false;
    UtilityFunctions::print("Stopping video stream");
    
    // TODO: Implement cleanup logic here
}

bool VideoStream::is_stream_active() const {
    return is_streaming;
}

void VideoStream::_ready() {
    UtilityFunctions::print("VideoStream node ready");
}

void VideoStream::_process(double delta) {
    if (is_streaming) {
        // TODO: Process new video frames here
        // This would run every frame while streaming
    }
}