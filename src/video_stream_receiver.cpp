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
	setup_tcp_connection();
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

void VideoStreamReceiver::setup_tcp_connection() {
	if (tcp_connection) {
		memdelete(tcp_connection);
	}
	
	tcp_connection = memnew(StreamPeerTCP);
}

void VideoStreamReceiver::setup_timer() {
	if (stream_timer) {
		stream_timer->queue_free();
	}
	
	stream_timer = memnew(Timer);
	stream_timer->set_wait_time(1.0 / 120.0); // Check for data 120 times per second
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
	update_connection_status("Connecting");
	is_streaming = true;
	
	// Initialize FPS tracking
	fps_update_time = Time::get_singleton()->get_unix_time_from_system();
	frame_count = 0;
	current_fps = 0.0f;
	
	// Clear buffers
	stream_buffer.clear();
	found_boundary = false;
	
	// Start TCP connection
	connect_to_server();
}

void VideoStreamReceiver::stop_stream() {
	if (is_streaming) {
		is_streaming = false;
		if (stream_timer) {
			stream_timer->stop();
		}
		// Properly reset TCP connection regardless of status
		if (tcp_connection) {
			StreamPeerTCP::Status status = tcp_connection->get_status();
			UtilityFunctions::print("Stopping stream, TCP status: ", status);
			if (status == StreamPeerTCP::STATUS_CONNECTED || status == StreamPeerTCP::STATUS_CONNECTING) {
				tcp_connection->disconnect_from_host();
			}
			// Create a new TCP connection to ensure clean state
			memdelete(tcp_connection);
			tcp_connection = memnew(StreamPeerTCP);
		}
		update_connection_status("Disconnected");
	}
}

void VideoStreamReceiver::connect_to_server() {
	if (!tcp_connection) {
		UtilityFunctions::print("TCP connection is null!");
		return;
	}
	
	UtilityFunctions::print("Attempting to connect to ", ip_address, ":", port);
	Error err = tcp_connection->connect_to_host(ip_address, port);
	if (err != OK) {
		UtilityFunctions::print("Connect error: ", err);
		update_connection_status("Connection Error");
		show_fallback_display();
		stop_stream();
		return;
	}
	
	UtilityFunctions::print("Connection initiated successfully");
	// Start timer to check connection and read data
	if (stream_timer) {
		stream_timer->start();
	}
}

void VideoStreamReceiver::send_http_request() {
	if (!tcp_connection || tcp_connection->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}
	
	// Send HTTP GET request for MJPEG stream
	String request = "GET / HTTP/1.1\r\n";
	request += "Host: " + ip_address + ":" + String::num_int64(port) + "\r\n";
	request += "Connection: keep-alive\r\n";
	request += "Cache-Control: no-cache\r\n";
	request += "\r\n";
	
	PackedByteArray request_data = request.to_utf8_buffer();
	tcp_connection->put_data(request_data);
	
	update_connection_status("Connected");
}

void VideoStreamReceiver::read_stream_data() {
	if (!tcp_connection || tcp_connection->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		return;
	}
	
	int available = tcp_connection->get_available_bytes();
	if (available > 0) {
		PackedByteArray data = tcp_connection->get_data(available)[1];
		stream_buffer.append_array(data);
		
		// Parse boundary marker from first response if not found
		if (!found_boundary) {
			String buffer_str = stream_buffer.get_string_from_utf8();
			int content_type_pos = buffer_str.find("Content-Type: multipart/x-mixed-replace");
			if (content_type_pos >= 0) {
				int boundary_pos = buffer_str.find("boundary=", content_type_pos);
				if (boundary_pos >= 0) {
					int boundary_end = buffer_str.find("\r\n", boundary_pos);
					if (boundary_end == -1) boundary_end = buffer_str.find("\n", boundary_pos);
					if (boundary_end >= 0) {
						boundary_marker = "--" + buffer_str.substr(boundary_pos + 9, boundary_end - boundary_pos - 9);
						found_boundary = true;
					}
				}
			}
		}
		
		process_mjpeg_stream();
	}
}

void VideoStreamReceiver::_on_stream_timer_timeout() {
	if (!is_streaming) {
		return;
	}
	
	if (!tcp_connection) {
		return;
	}
	
	StreamPeerTCP::Status status = tcp_connection->get_status();
	
	switch (status) {
		case StreamPeerTCP::STATUS_NONE:
			UtilityFunctions::print("TCP Status: NONE");
			update_connection_status("Connection Error");
			show_fallback_display();
			stop_stream();
			break;
		case StreamPeerTCP::STATUS_ERROR:
			UtilityFunctions::print("TCP Status: ERROR");
			update_connection_status("Connection Error");
			show_fallback_display();
			stop_stream();
			break;
			
		case StreamPeerTCP::STATUS_CONNECTING:
			UtilityFunctions::print("TCP Status: CONNECTING");
			update_connection_status("Connecting");
			break;
			
		case StreamPeerTCP::STATUS_CONNECTED:
			UtilityFunctions::print("TCP Status: CONNECTED");
			// Send HTTP request if we just connected
			if (connection_status == "Connecting") {
				UtilityFunctions::print("Sending HTTP request...");
				send_http_request();
			}
			// Read available data
			read_stream_data();
			break;
	}
}

void VideoStreamReceiver::process_mjpeg_stream() {
	if (!found_boundary) {
		return;
	}
	
	// Convert buffer to string for boundary searching
	String buffer_str = stream_buffer.get_string_from_utf8();
	
	while (true) {
		// Find current boundary
		int boundary_pos = buffer_str.find(boundary_marker);
		if (boundary_pos == -1) {
			break; // No complete frame yet
		}
		
		// Find next boundary
		int next_boundary = buffer_str.find(boundary_marker, boundary_pos + boundary_marker.length());
		if (next_boundary == -1) {
			break; // Frame not complete yet
		}
		
		// Extract frame data between boundaries
		String frame_section = buffer_str.substr(boundary_pos, next_boundary - boundary_pos);
		
		// Find end of headers (double CRLF)
		int header_end = frame_section.find("\r\n\r\n");
		if (header_end == -1) {
			header_end = frame_section.find("\n\n"); // Try just LF
		}
		
		if (header_end >= 0) {
			// Extract JPEG data
			int jpeg_start = boundary_pos + header_end + 4;
			int jpeg_length = next_boundary - jpeg_start;
			
			if (jpeg_length > 0) {
				PackedByteArray jpeg_data;
				jpeg_data.resize(jpeg_length);
				
				// Copy JPEG data
				for (int i = 0; i < jpeg_length && (jpeg_start + i) < stream_buffer.size(); i++) {
					jpeg_data[i] = stream_buffer[jpeg_start + i];
				}
				
				// Process this frame
				parse_jpeg_frame(jpeg_data);
			}
		}
		
		// Remove processed data from buffer
		PackedByteArray new_buffer;
		int remaining_start = next_boundary;
		int remaining_size = stream_buffer.size() - remaining_start;
		
		if (remaining_size > 0) {
			new_buffer.resize(remaining_size);
			for (int i = 0; i < remaining_size; i++) {
				new_buffer[i] = stream_buffer[remaining_start + i];
			}
		}
		
		stream_buffer = new_buffer;
		buffer_str = stream_buffer.get_string_from_utf8();
	}
}

void VideoStreamReceiver::parse_jpeg_frame(const PackedByteArray& jpeg_data) {
	if (jpeg_data.size() < 2) {
		return;
	}
	
	// Verify JPEG header
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
	
	// Update FPS calculation
	double current_time = Time::get_singleton()->get_unix_time_from_system();
	frame_count++;
	
	if (fps_update_time == 0.0) {
		fps_update_time = current_time;
	}
	
	if (current_time - fps_update_time >= 1.0) {
		current_fps = frame_count / (current_time - fps_update_time);
		frame_count = 0;
		fps_update_time = current_time;
		
		// Update Inspector display
		notify_property_list_changed();
	}
}

void VideoStreamReceiver::calculate_brightness(const Ref<Image>& image) {
	if (image.is_null()) {
		brightness_level = 0.0f;
		return;
	}
	
	PackedByteArray data = image->get_data();
	int pixel_count = image->get_width() * image->get_height();
	
	// Calculate histogram and advanced statistics
	int histogram[256] = {0};
	long long total_brightness = 0;
	
	// Build histogram and calculate mean
	for (int i = 0; i < data.size(); i += 3) {
		int gray = (data[i] + data[i + 1] + data[i + 2]) / 3;
		histogram[gray]++;
		total_brightness += gray;
	}
	
	float mean = (float)total_brightness / (float)pixel_count;
	
	// Calculate standard deviation for contrast measure
	float variance = 0.0f;
	for (int i = 0; i < data.size(); i += 3) {
		int gray = (data[i] + data[i + 1] + data[i + 2]) / 3;
		float diff = gray - mean;
		variance += diff * diff;
	}
	float std_dev = sqrt(variance / pixel_count);
	
	// Find percentiles for dynamic range analysis
	int cumulative = 0;
	int p5 = 0, p95 = 0, p50 = 0; // 5th, 95th, and 50th percentiles
	
	for (int i = 0; i < 256; i++) {
		cumulative += histogram[i];
		if (p5 == 0 && cumulative >= pixel_count * 0.05f) p5 = i;
		if (p50 == 0 && cumulative >= pixel_count * 0.50f) p50 = i;
		if (p95 == 0 && cumulative >= pixel_count * 0.95f) p95 = i;
	}
	
	// Advanced brightness assessment combining multiple factors
	float mean_norm = mean / 255.0f; // 0-1
	float contrast_norm = std_dev / 128.0f; // Higher std_dev = more contrast
	float dynamic_range = (p95 - p5) / 255.0f; // How much of the range is used
	
	float exposure_score = 0.0f;
	
	// Evaluate exposure based on mean
	if (mean_norm < 0.15f) {
		// Very underexposed
		exposure_score = -1.0f + (mean_norm / 0.15f) * 0.5f; // -1.0 to -0.5
	} else if (mean_norm < 0.35f) {
		// Slightly underexposed
		exposure_score = -0.5f + ((mean_norm - 0.15f) / 0.20f) * 0.5f; // -0.5 to 0.0
	} else if (mean_norm < 0.65f) {
		// Well exposed - factor in contrast and dynamic range
		float base_score = ((mean_norm - 0.35f) / 0.30f) * 0.4f - 0.2f; // -0.2 to 0.2
		
		// Boost score for good contrast
		if (contrast_norm > 0.25f) base_score += 0.1f;
		if (dynamic_range > 0.6f) base_score += 0.1f;
		
		exposure_score = base_score;
	} else if (mean_norm < 0.85f) {
		// Slightly overexposed
		exposure_score = 0.0f + ((mean_norm - 0.65f) / 0.20f) * 0.5f; // 0.0 to 0.5
	} else {
		// Very overexposed
		exposure_score = 0.5f + ((mean_norm - 0.85f) / 0.15f) * 0.5f; // 0.5 to 1.0
	}
	
	// Penalize low contrast (flat/washed out images)
	if (contrast_norm < 0.15f) {
		if (exposure_score > 0) exposure_score += 0.3f; // Push toward overexposed
		else exposure_score -= 0.3f; // Push toward underexposed
	}
	
	// Penalize poor dynamic range
	if (dynamic_range < 0.3f) {
		exposure_score *= 1.3f; // Amplify the problem
	}
	
	brightness_level = CLAMP(exposure_score, -1.0f, 1.0f);
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