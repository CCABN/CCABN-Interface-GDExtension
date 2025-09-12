#ifndef VIDEO_STREAM_H
#define VIDEO_STREAM_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

class VideoStream : public Control {
    GDCLASS(VideoStream, Control)

private:
    String stream_url;
    bool is_streaming;
    Ref<Texture2D> current_frame;

protected:
    static void _bind_methods();

public:
    VideoStream();
    ~VideoStream();

    void set_stream_url(const String &p_url);
    String get_stream_url() const;
    
    void start_stream();
    void stop_stream();
    bool is_stream_active() const;
    
    void _ready() override;
    void _process(double delta) override;
};

#endif // VIDEO_STREAM_H