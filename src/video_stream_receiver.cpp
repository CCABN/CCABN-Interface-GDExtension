#include "video_stream_receiver.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

VideoStreamReceiver::VideoStreamReceiver() {
	ip_address = "localhost";
	port = 8082;
	connection_status = "Ready";
	brightness_level = 0.0f;
	is_streaming = false;
	
	tcp_connection = nullptr;
	texture_rect = nullptr;
	stream_timer = nullptr;
	
	current_fps = 0.0f;
	last_frame_time = 0.0;
	frame_count = 0;
	fps_update_time = 0.0;
	
	boundary_marker = "--frameboundary";
	found_boundary = false;
}

VideoStreamReceiver::~VideoStreamReceiver() {
	stop_stream();
	if (tcp_connection) {
		memdelete(tcp_connection);
		tcp_connection = nullptr;
	}
}

void VideoStreamReceiver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ip_address", "address"), &VideoStreamReceiver::set_ip_address);
	ClassDB::bind_method(D_METHOD("get_ip_address"), &VideoStreamReceiver::get_ip_address);
	ClassDB::bind_method(D_METHOD("set_port", "port"), &VideoStreamReceiver::set_port);
	ClassDB::bind_method(D_METHOD("get_port"), &VideoStreamReceiver::get_port);
	ClassDB::bind_method(D_METHOD("get_connection_status"), &VideoStreamReceiver::get_connection_status);
	ClassDB::bind_method(D_METHOD("get_brightness_level"), &VideoStreamReceiver::get_brightness_level);
	ClassDB::bind_method(D_METHOD("get_current_fps"), &VideoStreamReceiver::get_current_fps);
	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoStreamReceiver::get_video_texture);
	ClassDB::bind_method(D_METHOD("start_stream_manual"), &VideoStreamReceiver::start_stream_manual);
	
	ClassDB::bind_method(D_METHOD("_on_stream_timer_timeout"), &VideoStreamReceiver::_on_stream_timer_timeout);
	
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ip_address"), "set_ip_address", "get_ip_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "connection_status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_connection_status");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brightness_level", PROPERTY_HINT_RANGE, "-1.0,1.0", PROPERTY_USAGE_READ_ONLY), "", "get_brightness_level");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_fps", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_current_fps");
}

void VideoStreamReceiver::_ready() {
	setup_ui();
	setup_timer();
	show_fallback_display();
	
	// Only start streaming in runtime, not in editor
	if (!Engine::get_singleton()->is_editor_hint() && !ip_address.is_empty()) {
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

void VideoStreamReceiver::setup_timer() {
	if (stream_timer) {
		stream_timer->queue_free();
	}
	
	stream_timer = memnew(Timer);
	stream_timer->set_wait_time(1.0 / 60.0); // 60 FPS polling
	stream_timer->set_autostart(false);
	add_child(stream_timer);
	stream_timer->connect("timeout", Callable(this, "_on_stream_timer_timeout"));
}

void VideoStreamReceiver::start_stream() {
	if (ip_address.is_empty()) {
		update_connection_status("No Address");
		return;
	}
	
	stop_stream();
	
	// Create fresh TCP connection
	tcp_connection = memnew(StreamPeerTCP);
	
	// Initialize FPS tracking
	fps_update_time = Time::get_singleton()->get_unix_time_from_system();
	frame_count = 0;
	current_fps = 0.0f;
	
	// Clear buffers
	stream_buffer.clear();
	found_boundary = false;
	
	// Attempt connection
	update_connection_status("Connecting");
	Error err = tcp_connection->connect_to_host(ip_address, port);
	if (err != OK) {
		UtilityFunctions::print("Failed to initiate connection: ", err);
		update_connection_status("Connection Failed");
		stop_stream();
		return;
	}
	
	is_streaming = true;
	stream_timer->start();
	UtilityFunctions::print("Starting stream to ", ip_address, ":", port);
}

void VideoStreamReceiver::stop_stream() {
	if (stream_timer) {
		stream_timer->stop();
	}
	
	if (tcp_connection) {
		tcp_connection->disconnect_from_host();
		memdelete(tcp_connection);
		tcp_connection = nullptr;
	}
	
	is_streaming = false;
	update_connection_status("Disconnected");
}

void VideoStreamReceiver::_on_stream_timer_timeout() {
	if (!is_streaming || !tcp_connection) {
		return;
	}
	
	StreamPeerTCP::Status status = tcp_connection->get_status();
	
	switch (status) {
		case StreamPeerTCP::STATUS_NONE:
			UtilityFunctions::print("TCP Status: NONE - Connection failed");
			update_connection_status("Connection Error");
			stop_stream();
			break;
		case StreamPeerTCP::STATUS_ERROR:
			UtilityFunctions::print("TCP Status: ERROR - Connection error");
			update_connection_status("Connection Error");
			stop_stream();
			break;
			
		case StreamPeerTCP::STATUS_CONNECTING:
			UtilityFunctions::print("TCP Status: CONNECTING - Still connecting...");
			update_connection_status("Connecting");
			break;
			
		case StreamPeerTCP::STATUS_CONNECTED:
			if (connection_status == "Connecting") {
				UtilityFunctions::print("TCP Status: CONNECTED - Connection established!");
				send_http_request();
			}
			read_stream_data();
			break;
	}
}

void VideoStreamReceiver::send_http_request() {
	UtilityFunctions::print("Connected! Sending HTTP request");
	
	String request = "GET / HTTP/1.1\r\n";
	request += "Host: " + ip_address + ":" + String::num_int64(port) + "\r\n";
	request += "Connection: keep-alive\r\n";
	request += "User-Agent: Godot/VideoStreamReceiver\r\n";
	request += "\r\n";
	
	PackedByteArray request_data = request.to_utf8_buffer();
	tcp_connection->put_data(request_data);
	
	update_connection_status("Connected");
}

void VideoStreamReceiver::read_stream_data() {
	int available = tcp_connection->get_available_bytes();
	if (available <= 0) {
		return;
	}
	
	UtilityFunctions::print("Reading ", available, " bytes of data");
	Array result = tcp_connection->get_data(available);
	Error err = static_cast<Error>(result[0].operator int());
	
	if (err != OK) {
		UtilityFunctions::print("Error reading data: ", err);
		return;
	}
	
	PackedByteArray data = result[1];
	stream_buffer.append_array(data);
	UtilityFunctions::print("Total buffer size now: ", stream_buffer.size());
	
	// Debug: Print first 200 characters of buffer to see what we're getting
	if (stream_buffer.size() > 0) {
		String buffer_preview = stream_buffer.get_string_from_utf8().substr(0, 200);
		UtilityFunctions::print("Buffer preview: ", buffer_preview);
	}
	
	// Try to find boundary in HTTP headers if not found yet
	if (!found_boundary) {
		String buffer_str = stream_buffer.get_string_from_utf8();
		int content_type_pos = buffer_str.find("Content-Type:");
		if (content_type_pos >= 0) {
			UtilityFunctions::print("Found Content-Type header");
			int boundary_pos = buffer_str.find("boundary=", content_type_pos);
			if (boundary_pos >= 0) {
				int line_end = buffer_str.find("\r\n", boundary_pos);
				if (line_end == -1) line_end = buffer_str.find("\n", boundary_pos);
				if (line_end >= 0) {
					String boundary_value = buffer_str.substr(boundary_pos + 9, line_end - boundary_pos - 9);
					boundary_marker = "--" + boundary_value.strip_edges();
					found_boundary = true;
					UtilityFunctions::print("Found boundary: ", boundary_marker);
				}
			}
		}
	}
	
	// Process MJPEG frames if we have a boundary
	if (found_boundary) {
		UtilityFunctions::print("Processing MJPEG stream...");
		process_mjpeg_stream();
	}
}

void VideoStreamReceiver::process_mjpeg_stream() {
	// Convert to string for easier boundary searching
	String buffer_str = stream_buffer.get_string_from_utf8();
	
	while (true) {
		int boundary_start = buffer_str.find(boundary_marker);
		if (boundary_start == -1) break;
		
		int next_boundary = buffer_str.find(boundary_marker, boundary_start + boundary_marker.length());
		if (next_boundary == -1) break;
		
		// Find end of headers in this frame section
		String frame_section = buffer_str.substr(boundary_start, next_boundary - boundary_start);
		int header_end = frame_section.find("\r\n\r\n");
		if (header_end == -1) {
			header_end = frame_section.find("\n\n");
		}
		
		if (header_end >= 0) {
			int jpeg_start_offset = header_end + 4; // After header end marker
			if (header_end == frame_section.find("\n\n")) {
				jpeg_start_offset = header_end + 2; // For \n\n instead of \r\n\r\n
			}
			
			int jpeg_start = boundary_start + jpeg_start_offset;
			int jpeg_length = next_boundary - jpeg_start;
			
			if (jpeg_length > 0 && jpeg_start >= 0 && (jpeg_start + jpeg_length) <= stream_buffer.size()) {
				PackedByteArray jpeg_data;
				jpeg_data.resize(jpeg_length);
				for (int i = 0; i < jpeg_length; i++) {
					jpeg_data[i] = stream_buffer[jpeg_start + i];
				}
				parse_jpeg_frame(jpeg_data);
			}
		}
		
		// Remove processed data from buffer
		int remaining_size = stream_buffer.size() - next_boundary;
		if (remaining_size > 0) {
			PackedByteArray new_buffer;
			new_buffer.resize(remaining_size);
			for (int i = 0; i < remaining_size; i++) {
				new_buffer[i] = stream_buffer[next_boundary + i];
			}
			stream_buffer = new_buffer;
			buffer_str = stream_buffer.get_string_from_utf8();
		} else {
			stream_buffer.clear();
			break;
		}
	}
}

void VideoStreamReceiver::parse_jpeg_frame(const PackedByteArray& jpeg_data) {
	UtilityFunctions::print("Attempting to parse JPEG frame of size: ", jpeg_data.size());
	
	if (jpeg_data.size() < 2 || jpeg_data[0] != 0xFF || jpeg_data[1] != 0xD8) {
		UtilityFunctions::print("Invalid JPEG header");
		return;
	}
	
	Ref<Image> image = memnew(Image);
	Error err = image->load_jpg_from_buffer(jpeg_data);
	if (err != OK) {
		UtilityFunctions::print("Failed to load JPEG: ", err);
		return;
	}
	
	UtilityFunctions::print("Successfully loaded image: ", image->get_width(), "x", image->get_height());
	
	// Resize to expected dimensions
	if (image->get_width() != 240 || image->get_height() != 240) {
		image->resize(240, 240, Image::INTERPOLATE_NEAREST);
	}
	
	if (image->get_format() != Image::FORMAT_RGB8) {
		image->convert(Image::FORMAT_RGB8);
	}
	
	calculate_brightness(image);
	update_display_texture(image);
	
	// Update FPS
	frame_count++;
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	if (current_time - fps_update_time >= 1.0) {
		current_fps = frame_count / (current_time - fps_update_time);
		frame_count = 0;
		fps_update_time = current_time;
		notify_property_list_changed();
		UtilityFunctions::print("FPS: ", current_fps);
	}
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
	
	float mean = (float)total_brightness / (float)pixel_count;
	float mean_norm = mean / 255.0f;
	
	// Simple brightness assessment: -1 (dark) to +1 (bright)
	if (mean_norm < 0.3f) {
		brightness_level = -1.0f + (mean_norm / 0.3f);
	} else if (mean_norm > 0.7f) {
		brightness_level = (mean_norm - 0.7f) / 0.3f;
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

float VideoStreamReceiver::get_current_fps() const {
	return current_fps;
}

void VideoStreamReceiver::start_stream_manual() {
	if (!Engine::get_singleton()->is_editor_hint() && !ip_address.is_empty()) {
		start_stream();
	}
}