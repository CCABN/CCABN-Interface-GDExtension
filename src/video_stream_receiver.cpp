#include "video_stream_receiver.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

VideoStreamReceiver::VideoStreamReceiver() {
	ip_address = "";
	port = 8080;
	connection_status = "No Address";
	brightness_level = 0.0f;
	is_streaming = false;
	
	http_request = nullptr;
	texture_rect = nullptr;
	request_timer = nullptr;
}

VideoStreamReceiver::~VideoStreamReceiver() {
	stop_stream();
}

void VideoStreamReceiver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ip_address", "address"), &VideoStreamReceiver::set_ip_address);
	ClassDB::bind_method(D_METHOD("get_ip_address"), &VideoStreamReceiver::get_ip_address);
	ClassDB::bind_method(D_METHOD("set_port", "port"), &VideoStreamReceiver::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &VideoStreamReceiver::get_port);
	ClassDB::bind_method(D_METHOD("get_connection_status"), &VideoStreamReceiver::get_connection_status);
	ClassDB::bind_method(D_METHOD("get_brightness_level"), &VideoStreamReceiver::get_brightness_level);
	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoStreamReceiver::get_video_texture);
	ClassDB::bind_method(D_METHOD("start_stream_manual"), &VideoStreamReceiver::start_stream_manual);
	
	ClassDB::bind_method(D_METHOD("_on_http_request_completed", "result", "response_code", "headers", "body"), &VideoStreamReceiver::_on_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_request_timer_timeout"), &VideoStreamReceiver::_on_request_timer_timeout);
	
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ip_address"), "set_ip_address", "get_ip_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "connection_status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_connection_status");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brightness_level", PROPERTY_HINT_RANGE, "-1.0,1.0", PROPERTY_USAGE_READ_ONLY), "", "get_brightness_level");
}

void VideoStreamReceiver::_ready() {
	setup_ui();
	setup_http_request();
	setup_timer();
	show_fallback_display();
	
	if (!ip_address.is_empty()) {
		start_stream();
	}
}

void VideoStreamReceiver::_enter_tree() {
	set_custom_minimum_size(Vector2(240, 240));
}

void VideoStreamReceiver::_exit_tree() {
	stop_stream();
}

void VideoStreamReceiver::setup_ui() {
	texture_rect = memnew(TextureRect);
	texture_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	texture_rect->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
	texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	add_child(texture_rect);
	
	fallback_image = Image::create(240, 240, false, Image::FORMAT_RGB8);
	fallback_image->fill(Color(0.2f, 0.2f, 0.2f));
}

void VideoStreamReceiver::setup_http_request() {
	if (http_request) {
		http_request->queue_free();
	}
	
	http_request = memnew(HTTPRequest);
	add_child(http_request);
	http_request->connect("request_completed", Callable(this, "_on_http_request_completed"));
}

void VideoStreamReceiver::setup_timer() {
	if (request_timer) {
		request_timer->queue_free();
	}
	
	request_timer = memnew(Timer);
	request_timer->set_wait_time(0.016f); // ~60 FPS
	request_timer->set_autostart(false);
	request_timer->connect("timeout", Callable(this, "_on_request_timer_timeout"));
	add_child(request_timer);
}

void VideoStreamReceiver::start_stream() {
	if (ip_address.is_empty()) {
		update_connection_status("No Address");
		return;
	}
	
	stop_stream();
	update_connection_status("Connecting");
	is_streaming = true;
	request_frame();
	request_timer->start();
}

void VideoStreamReceiver::stop_stream() {
	if (is_streaming) {
		is_streaming = false;
		if (request_timer) {
			request_timer->stop();
		}
		if (http_request) {
			http_request->cancel_request();
		}
		update_connection_status("Disconnected");
	}
}

void VideoStreamReceiver::request_frame() {
	if (!is_streaming || !http_request) {
		return;
	}
	
	String url = "http://" + ip_address + ":" + String::num_int64(port);
	Error err = http_request->request(url);
	if (err != OK && err != ERR_BUSY) {
		update_connection_status("Connection Error");
		show_fallback_display();
		stop_stream();
	}
}

void VideoStreamReceiver::_on_request_timer_timeout() {
	request_frame();
}

void VideoStreamReceiver::_on_http_request_completed(int result, int response_code, const PackedStringArray& headers, const PackedByteArray& body) {
	if (!is_streaming) {
		return;
	}
	
	if (result != HTTPRequest::RESULT_SUCCESS) {
		update_connection_status("Request Failed");
		show_fallback_display();
		return;
	}
	
	if (response_code != 200) {
		update_connection_status("HTTP Error " + String::num(response_code));
		show_fallback_display();
		return;
	}
	
	bool is_jpeg = false;
	for (int i = 0; i < headers.size(); i++) {
		String header = headers[i].to_lower();
		if (header.begins_with("content-type") && header.contains("image/jpeg")) {
			is_jpeg = true;
			break;
		}
	}
	
	if (!is_jpeg) {
		update_connection_status("Invalid Image Format");
		show_fallback_display();
		return;
	}
	
	update_connection_status("Connected");
	parse_jpeg_frame(body);
}

void VideoStreamReceiver::parse_jpeg_frame(const PackedByteArray& jpeg_data) {
	if (jpeg_data.size() < 2) {
		return;
	}
	
	if (jpeg_data[0] != 0xFF || jpeg_data[1] != 0xD8) {
		return;
	}
	
	Ref<Image> image = memnew(Image);
	Error err = image->load_jpg_from_buffer(jpeg_data);
	if (err != OK) {
		return;
	}
	
	if (image->get_width() != 240 || image->get_height() != 240) {
		image->resize(240, 240, Image::INTERPOLATE_NEAREST);
	}
	
	if (image->get_format() != Image::FORMAT_RGB8) {
		image->convert(Image::FORMAT_RGB8);
	}
	
	calculate_brightness(image);
	update_display_texture(image);
}

void VideoStreamReceiver::calculate_brightness(const Ref<Image>& image) {
	if (image.is_null()) {
		brightness_level = 0.0f;
		return;
	}
	
	PackedByteArray data = image->get_data();
	int pixel_count = image->get_width() * image->get_height();
	long long total_brightness = 0;
	
	for (int i = 0; i < data.size(); i += 3) {
		int gray = (data[i] + data[i + 1] + data[i + 2]) / 3;
		total_brightness += gray;
	}
	
	float average_brightness = (float)total_brightness / (float)pixel_count;
	float normalized = (average_brightness / 255.0f) * 2.0f - 1.0f;
	
	float optimal_range = 0.3f;
	if (normalized < -optimal_range) {
		brightness_level = -1.0f + (normalized + optimal_range) / (1.0f - optimal_range);
	} else if (normalized > optimal_range) {
		brightness_level = (normalized - optimal_range) / (1.0f - optimal_range);
	} else {
		brightness_level = 0.0f;
	}
	
	brightness_level = CLAMP(brightness_level, -1.0f, 1.0f);
}

void VideoStreamReceiver::update_display_texture(const Ref<Image>& image) {
	if (image.is_null()) {
		show_fallback_display();
		return;
	}
	
	if (current_texture.is_null()) {
		current_texture = ImageTexture::create_from_image(image);
	} else {
		current_texture->update(image);
	}
	
	if (texture_rect) {
		texture_rect->set_texture(current_texture);
	}
}

void VideoStreamReceiver::show_fallback_display() {
	if (current_texture.is_null()) {
		current_texture = ImageTexture::create_from_image(fallback_image);
	} else {
		current_texture->update(fallback_image);
	}
	
	if (texture_rect) {
		texture_rect->set_texture(current_texture);
	}
	
	brightness_level = 0.0f;
}

void VideoStreamReceiver::update_connection_status(const String& status) {
	connection_status = status;
	notify_property_list_changed();
}

void VideoStreamReceiver::set_ip_address(const String& address) {
	if (ip_address != address) {
		ip_address = address;
		if (address.is_empty()) {
			stop_stream();
			show_fallback_display();
			update_connection_status("No Address");
		}
	}
}

String VideoStreamReceiver::get_ip_address() const {
	return ip_address;
}

void VideoStreamReceiver::set_port(int p_port) {
	if (port != p_port) {
		port = p_port;
	}
}

int VideoStreamReceiver::get_port() const {
	return port;
}

String VideoStreamReceiver::get_connection_status() const {
	return connection_status;
}

float VideoStreamReceiver::get_brightness_level() const {
	return brightness_level;
}

Ref<ImageTexture> VideoStreamReceiver::get_video_texture() const {
	return current_texture;
}

void VideoStreamReceiver::start_stream_manual() {
	if (!ip_address.is_empty()) {
		start_stream();
	}
}