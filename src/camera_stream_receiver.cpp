#include "camera_stream_receiver.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/time.hpp>

using namespace godot;

void CameraStreamReceiver::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("connect_to_server"), &CameraStreamReceiver::connect_to_server);
    ClassDB::bind_method(D_METHOD("connect_to_server_url", "url"), &CameraStreamReceiver::connect_to_server_url);
    ClassDB::bind_method(D_METHOD("disconnect_from_server"), &CameraStreamReceiver::disconnect_from_server);

    // Properties
    ClassDB::bind_method(D_METHOD("set_server_url", "url"), &CameraStreamReceiver::set_server_url);
    ClassDB::bind_method(D_METHOD("get_server_url"), &CameraStreamReceiver::get_server_url);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_url", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_server_url", "get_server_url");

    ClassDB::bind_method(D_METHOD("set_auto_connect", "enabled"), &CameraStreamReceiver::set_auto_connect);
    ClassDB::bind_method(D_METHOD("get_auto_connect"), &CameraStreamReceiver::get_auto_connect);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_connect", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_auto_connect", "get_auto_connect");

    ClassDB::bind_method(D_METHOD("get_connection_state"), &CameraStreamReceiver::get_connection_state);
    ClassDB::bind_method(D_METHOD("get_connection_state_string"), &CameraStreamReceiver::get_connection_state_string);

    ClassDB::bind_method(D_METHOD("get_fps"), &CameraStreamReceiver::get_fps);

    ClassDB::bind_method(D_METHOD("get_texture"), &CameraStreamReceiver::get_texture);

    // LED control
    ClassDB::bind_method(D_METHOD("set_led_brightness", "brightness"), &CameraStreamReceiver::set_led_brightness);
    ClassDB::bind_method(D_METHOD("get_led_brightness"), &CameraStreamReceiver::get_led_brightness);

    // Signals
    ADD_SIGNAL(MethodInfo("frame_received", PropertyInfo(Variant::OBJECT, "texture")));
    ADD_SIGNAL(MethodInfo("connected"));
    ADD_SIGNAL(MethodInfo("disconnected"));
    ADD_SIGNAL(MethodInfo("connection_error", PropertyInfo(Variant::STRING, "message")));

    // Expose connection state constants as integers
    BIND_CONSTANT(CONNECTION_STATE_DISCONNECTED);
    BIND_CONSTANT(CONNECTION_STATE_CONNECTING);
    BIND_CONSTANT(CONNECTION_STATE_CONNECTED);
    BIND_CONSTANT(CONNECTION_STATE_ERROR);
}

CameraStreamReceiver::CameraStreamReceiver() {
    ws_peer.instantiate();

    // Configure WebSocket for larger binary messages (JPEG frames)
    ws_peer->set_inbound_buffer_size(65536);   // 64KB inbound buffer
    ws_peer->set_outbound_buffer_size(16384);  // 16KB outbound buffer
    ws_peer->set_max_queued_packets(32);       // Allow more queued packets

    texture.instantiate();
    current_image.instantiate();
    UtilityFunctions::print("[CameraStreamReceiver:", (uint64_t)this, "] Constructor - server_url = '", server_url, "'");
}

CameraStreamReceiver::~CameraStreamReceiver() {
    disconnect_from_server();
}

void CameraStreamReceiver::_ready() {
    UtilityFunctions::print("[CameraStreamReceiver:", (uint64_t)this, "] _ready() - server_url = '", server_url, "', auto_connect = ", auto_connect);

    if (auto_connect) {
        UtilityFunctions::print("[CameraStreamReceiver:", (uint64_t)this, "] Auto-connecting to '", server_url, "'");
        connect_to_server();
    }
}

void CameraStreamReceiver::_process(double delta) {
    _update_fps_counter(delta);

    switch (connection_state) {
        case CONNECTION_STATE_DISCONNECTED:
            _handle_reconnection(delta);
            break;

        case CONNECTION_STATE_CONNECTING:
        case CONNECTION_STATE_CONNECTED:
            _poll_websocket();
            break;

        case CONNECTION_STATE_ERROR:
            _handle_reconnection(delta);
            break;
    }
}

void CameraStreamReceiver::connect_to_server() {
    UtilityFunctions::print("[CameraStreamReceiver:", (uint64_t)this, "] connect_to_server() called - server_url = '", server_url, "'");
    connect_to_server_url(server_url);
}

void CameraStreamReceiver::connect_to_server_url(const String& url) {
    if (connection_state == CONNECTION_STATE_CONNECTED || connection_state == CONNECTION_STATE_CONNECTING) {
        UtilityFunctions::print("[CameraStreamReceiver] Already connected or connecting");
        return;
    }

    // Make a copy to avoid self-assignment issues when called with server_url as parameter
    String url_to_use = url;

    // Only update server_url if url is not empty
    if (!url_to_use.is_empty()) {
        server_url = url_to_use;
    }

    // Double-check we have a valid URL before attempting connection
    if (server_url.is_empty()) {
        UtilityFunctions::printerr("[CameraStreamReceiver] Cannot connect: server_url is empty");
        connection_state = CONNECTION_STATE_ERROR;
        emit_signal("connection_error", "Server URL is empty");
        return;
    }

    connection_state = CONNECTION_STATE_CONNECTING;

    UtilityFunctions::print("[CameraStreamReceiver] Connecting to ", server_url);

    Error err = ws_peer->connect_to_url(server_url);
    if (err != OK) {
        UtilityFunctions::printerr("[CameraStreamReceiver] Failed to initiate connection: ", err);
        connection_state = CONNECTION_STATE_ERROR;
        emit_signal("connection_error", "Failed to initiate connection");
    }
}

void CameraStreamReceiver::disconnect_from_server() {
    if (connection_state != CONNECTION_STATE_DISCONNECTED) {
        ws_peer->close();
        connection_state = CONNECTION_STATE_DISCONNECTED;
        UtilityFunctions::print("[CameraStreamReceiver] Disconnected");
        emit_signal("disconnected");
    }
}

void CameraStreamReceiver::_poll_websocket() {
    ws_peer->poll();

    WebSocketPeer::State state = ws_peer->get_ready_state();

    int available = ws_peer->get_available_packet_count();
    static int poll_counter = 0;
    static int last_available = 0;

    // Log when packet count changes or periodically
    if (available != last_available || poll_counter++ % 100 == 0) {
        UtilityFunctions::print("[CameraStreamReceiver] _poll_websocket: state=", (int)state,
                               ", available_packets=", available,
                               ", ready_state=", ws_peer->get_ready_state());
        last_available = available;
    }

    switch (state) {
        case WebSocketPeer::STATE_OPEN: {
            if (connection_state != CONNECTION_STATE_CONNECTED) {
                connection_state = CONNECTION_STATE_CONNECTED;
                current_reconnect_delay = reconnect_delay;  // Reset reconnect delay
                UtilityFunctions::print("[CameraStreamReceiver] Connected!");
                emit_signal("connected");
            }

            // Process all available packets
            int packet_count = ws_peer->get_available_packet_count();
            if (packet_count > 0) {
                UtilityFunctions::print("[CameraStreamReceiver] Processing ", packet_count, " packets");
            }

            while (ws_peer->get_available_packet_count() > 0) {
                PackedByteArray packet = ws_peer->get_packet();
                UtilityFunctions::print("[CameraStreamReceiver] Received packet: size=", packet.size(),
                                       ", is_string=", ws_peer->was_string_packet());
                if (packet.size() > 0) {
                    _handle_packet(packet);
                }
            }

            // Check for timeout (no frames for 3 seconds)
            if (last_frame_time > 0.0f && (Time::get_singleton()->get_ticks_msec() / 1000.0f - last_frame_time) > connection_timeout) {
                UtilityFunctions::print("[CameraStreamReceiver] Connection timeout - no frames received");
                connection_state = CONNECTION_STATE_ERROR;
                emit_signal("connection_error", "Connection timeout");
            }
            break;
        }

        case WebSocketPeer::STATE_CONNECTING:
            // Still connecting, wait...
            break;

        case WebSocketPeer::STATE_CLOSING:
        case WebSocketPeer::STATE_CLOSED:
            if (connection_state != CONNECTION_STATE_DISCONNECTED) {
                UtilityFunctions::print("[CameraStreamReceiver] Connection closed");
                connection_state = CONNECTION_STATE_ERROR;
                emit_signal("disconnected");
            }
            break;
    }
}

void CameraStreamReceiver::_handle_packet(const PackedByteArray& packet) {
    // Update last frame time
    last_frame_time = Time::get_singleton()->get_ticks_msec() / 1000.0f;

    // Check if this is a text packet (JSON response) or binary (JPEG frame)
    if (ws_peer->was_string_packet()) {
        // JSON response (e.g., LED command acknowledgment)
        String response = String::utf8((const char*)packet.ptr(), packet.size());
        UtilityFunctions::print("[CameraStreamReceiver] Received text: ", response);
    } else {
        // Binary packet - raw RGB565 frame data
        // RGB565 is 160x120 = 19200 pixels * 2 bytes = 38400 bytes
        const int WIDTH = 160;
        const int HEIGHT = 120;
        const int EXPECTED_SIZE = WIDTH * HEIGHT * 2;

        if (packet.size() != EXPECTED_SIZE) {
            UtilityFunctions::printerr("[CameraStreamReceiver] Unexpected frame size: ", packet.size(),
                                      " (expected ", EXPECTED_SIZE, ")");
            return;
        }

        // Create RGB8 image from RGB565 data
        current_image->set_data(WIDTH, HEIGHT, false, Image::FORMAT_RGB8, PackedByteArray());

        // Convert RGB565 to RGB8
        PackedByteArray rgb8_data;
        rgb8_data.resize(WIDTH * HEIGHT * 3);

        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            uint16_t rgb565 = (packet[i * 2 + 1] << 8) | packet[i * 2];

            // Extract RGB components from RGB565
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  // 5 bits -> 8 bits
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   // 6 bits -> 8 bits
            uint8_t b = (rgb565 & 0x1F) << 3;          // 5 bits -> 8 bits

            rgb8_data[i * 3 + 0] = r;
            rgb8_data[i * 3 + 1] = g;
            rgb8_data[i * 3 + 2] = b;
        }

        current_image->set_data(WIDTH, HEIGHT, false, Image::FORMAT_RGB8, rgb8_data);

        UtilityFunctions::print("[CameraStreamReceiver] Converted RGB565 frame to RGB8");

        // Update texture with new image
        texture->update(current_image);

        // Emit signal with updated texture
        emit_signal("frame_received", texture);

        // Track FPS
        frames_received++;
    }
}

void CameraStreamReceiver::_handle_reconnection(double delta) {
    if (!auto_connect) {
        return;
    }

    static float reconnect_timer = 0.0f;
    reconnect_timer += delta;

    if (reconnect_timer >= current_reconnect_delay) {
        reconnect_timer = 0.0f;

        UtilityFunctions::print("[CameraStreamReceiver] Attempting reconnection...");
        connect_to_server();

        // Exponential backoff
        current_reconnect_delay = MIN(current_reconnect_delay * 2.0f, max_reconnect_delay);
    }
}

void CameraStreamReceiver::_send_led_command(float brightness) {
    if (connection_state != CONNECTION_STATE_CONNECTED) {
        UtilityFunctions::print("[CameraStreamReceiver] Not connected, cannot send LED command");
        return;
    }

    // Clamp brightness
    brightness = CLAMP(brightness, 0.0f, 1.0f);

    // Create JSON command: {"cmd":"led","brightness":0.5}
    String json_cmd = "{\"cmd\":\"led\",\"brightness\":" + String::num(brightness, 2) + "}";

    // Send as text packet
    Error err = ws_peer->send_text(json_cmd);
    if (err != OK) {
        UtilityFunctions::printerr("[CameraStreamReceiver] Failed to send LED command: ", err);
    } else {
        led_brightness = brightness;
        UtilityFunctions::print("[CameraStreamReceiver] Sent LED brightness: ", brightness);
    }
}

void CameraStreamReceiver::_update_fps_counter(double delta) {
    fps_timer += delta;

    if (fps_timer >= 1.0f) {
        current_fps = frames_received / fps_timer;
        frames_received = 0;
        fps_timer = 0.0f;
    }
}

// Getters/Setters

void CameraStreamReceiver::set_server_url(const String& url) {
    UtilityFunctions::print("[CameraStreamReceiver:", (uint64_t)this, "] set_server_url - OLD: '", server_url, "' -> NEW: '", url, "'");
    server_url = url;
}

String CameraStreamReceiver::get_server_url() const {
    return server_url;
}

void CameraStreamReceiver::set_auto_connect(bool enabled) {
    auto_connect = enabled;
}

bool CameraStreamReceiver::get_auto_connect() const {
    return auto_connect;
}

int CameraStreamReceiver::get_connection_state() const {
    return static_cast<int>(connection_state);
}

String CameraStreamReceiver::get_connection_state_string() const {
    switch (connection_state) {
        case CONNECTION_STATE_DISCONNECTED: return "DISCONNECTED";
        case CONNECTION_STATE_CONNECTING: return "CONNECTING";
        case CONNECTION_STATE_CONNECTED: return "CONNECTED";
        case CONNECTION_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

float CameraStreamReceiver::get_fps() const {
    return current_fps;
}

Ref<ImageTexture> CameraStreamReceiver::get_texture() const {
    return texture;
}

void CameraStreamReceiver::set_led_brightness(float brightness) {
    _send_led_command(brightness);
}

float CameraStreamReceiver::get_led_brightness() const {
    return led_brightness;
}
