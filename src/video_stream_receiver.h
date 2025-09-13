#ifndef VIDEO_STREAM_RECEIVER_H
#define VIDEO_STREAM_RECEIVER_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

class VideoStreamReceiver : public Control {
	GDCLASS(VideoStreamReceiver, Control)

private:
	HTTPRequest* http_request;
	TextureRect* texture_rect;
	Timer* request_timer;
	
	String ip_address;
	int port;
	String connection_status;
	float brightness_level;
	bool is_streaming;
	
	Ref<ImageTexture> current_texture;
	Ref<Image> fallback_image;
	
	void setup_ui();
	void setup_http_request();
	void setup_timer();
	void start_stream();
	void stop_stream();
	void request_frame();
	void parse_jpeg_frame(const PackedByteArray& jpeg_data);
	void calculate_brightness(const Ref<Image>& image);
	void update_display_texture(const Ref<Image>& image);
	void show_fallback_display();
	void update_connection_status(const String& status);

protected:
	static void _bind_methods();

public:
	VideoStreamReceiver();
	~VideoStreamReceiver();
	
	void _ready() override;
	void _enter_tree() override;
	void _exit_tree() override;
	
	void set_ip_address(const String& address);
	String get_ip_address() const;
	
	void set_port(int p_port);
	int get_port() const;
	
	String get_connection_status() const;
	float get_brightness_level() const;
	
	Ref<ImageTexture> get_video_texture() const;
	
	void start_stream_manual();
	
	void _on_http_request_completed(int result, int response_code, const PackedStringArray& headers, const PackedByteArray& body);
	void _on_request_timer_timeout();
};

#endif