#include "video_stream_receiver.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

VideoStreamReceiver::VideoStreamReceiver() {
	ip_address = "";
	port = 8080;
	connection_status = "No Address";
	brightness_level = 0.0f;
	is_streaming = false;
	
	http_request = nullptr;
	texture_rect = nullptr;
	request_timer = nullptr;
	
	current_fps = 0.0f;
	last_frame_time = 0.0;
	frame_count = 0;
	fps_update_time = 0.0;
	
	boundary_marker = "--frameboundary";
	found_boundary = false;
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
	ClassDB::bind_method(D_METHOD("get_current_fps"), &VideoStreamReceiver::get_current_fps);
	ClassDB::bind_method(D_METHOD("get_video_texture"), &VideoStreamReceiver::get_video_texture);
	ClassDB::bind_method(D_METHOD("start_stream_manual"), &VideoStreamReceiver::start_stream_manual);
	
	ClassDB::bind_method(D_METHOD("_on_http_request_completed", "result", "response_code", "headers", "body"), &VideoStreamReceiver::_on_http_request_completed);
	
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ip_address"), "set_ip_address", "get_ip_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "connection_status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_connection_status");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brightness_level", PROPERTY_HINT_RANGE, "-1.0,1.0", PROPERTY_USAGE_READ_ONLY), "", "get_brightness_level");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "current_fps", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_current_fps");
}

void VideoStreamReceiver::_ready() {
	setup_ui();
	setup_http_request();
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

void VideoStreamReceiver::setup_http_request() {
	if (http_request) {
		http_request->queue_free();
	}
	
	http_request = memnew(HTTPRequest);
	// Set streaming mode for continuous data
	http_request->set_download_chunk_size(8192); // Smaller chunks for faster processing
	add_child(http_request);
	http_request->connect("request_completed", Callable(this, "_on_http_request_completed"));
}

void VideoStreamReceiver::setup_timer() {
	// No timer needed for MJPEG streaming - data comes continuously
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
	
	// Start MJPEG stream request
	String url = "http://" + ip_address + ":" + String::num_int64(port);
	Error err = http_request->request(url);
	if (err != OK) {
		update_connection_status("Connection Error");
		show_fallback_display();
		stop_stream();
	}
}

void VideoStreamReceiver::stop_stream() {
	if (is_streaming) {
		is_streaming = false;
		if (http_request) {
			http_request->cancel_request();
		}
		update_connection_status("Disconnected");
	}
}

void VideoStreamReceiver::request_frame() {
	// Not used in MJPEG streaming - data comes continuously
}

void VideoStreamReceiver::_on_request_timer_timeout() {
	// Not used in MJPEG streaming
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
	
	// Check for MJPEG content type
	bool is_mjpeg = false;
	for (int i = 0; i < headers.size(); i++) {
		String header = headers[i].to_lower();
		if (header.begins_with("content-type") && header.contains("multipart/x-mixed-replace")) {
			is_mjpeg = true;
			// Extract boundary if not found
			if (!found_boundary) {
				int boundary_pos = header.find("boundary=");
				if (boundary_pos >= 0) {
					boundary_marker = header.substr(boundary_pos + 9);
					if (!boundary_marker.begins_with("--")) {
						boundary_marker = "--" + boundary_marker;
					}
					found_boundary = true;
				}
			}
			break;
		}
	}
	
	if (!is_mjpeg) {
		update_connection_status("Invalid Stream Format");
		show_fallback_display();
		return;
	}
	
	update_connection_status("Connected");
	
	// Process MJPEG stream data
	stream_buffer.append_array(body);
	process_mjpeg_stream();
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