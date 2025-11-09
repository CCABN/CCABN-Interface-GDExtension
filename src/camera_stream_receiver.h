#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/web_socket_peer.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

/**
 * @brief Receives camera stream from ESP32-S3 via WebSocket
 *
 * Features:
 * - Connects to ESP32 WebSocket server (ws://ccabn-tracker.local/stream)
 * - Receives grayscale JPEG frames at ~45 FPS
 * - Decodes JPEG using Godot's built-in Image::load_jpg_from_buffer()
 * - Updates ImageTexture for display
 * - Bidirectional LED brightness control
 * - Automatic reconnection with exponential backoff
 * - Connection state tracking
 */
class CameraStreamReceiver : public Node {
    GDCLASS(CameraStreamReceiver, Node)

public:
    enum ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR_STATE
    };

private:
    // WebSocket connection
    Ref<WebSocketPeer> ws_peer;
    String server_url = "ws://ccabn-tracker.local/stream";
    ConnectionState connection_state = DISCONNECTED;

    // Texture management
    Ref<ImageTexture> texture;
    Ref<Image> current_image;

    // Connection settings
    bool auto_connect = true;
    float reconnect_delay = 1.0f;
    float max_reconnect_delay = 10.0f;
    float current_reconnect_delay = 1.0f;

    // Frame rate tracking
    int frames_received = 0;
    float fps_timer = 0.0f;
    float current_fps = 0.0f;

    // Timeout detection
    float last_frame_time = 0.0f;
    float connection_timeout = 3.0f;

    // LED brightness state
    float led_brightness = 0.0f;

protected:
    static void _bind_methods();

public:
    CameraStreamReceiver();
    ~CameraStreamReceiver();

    // Godot lifecycle
    void _ready() override;
    void _process(double delta) override;

    // Connection management
    void connect_to_server();
    void connect_to_server_url(const String& url);
    void disconnect_from_server();

    // Getters/Setters
    void set_server_url(const String& url);
    String get_server_url() const;

    void set_auto_connect(bool enabled);
    bool get_auto_connect() const;

    ConnectionState get_connection_state() const;
    String get_connection_state_string() const;

    float get_fps() const;

    Ref<ImageTexture> get_texture() const;

    // LED control
    void set_led_brightness(float brightness);
    float get_led_brightness() const;

private:
    void _poll_websocket();
    void _handle_packet(const PackedByteArray& packet);
    void _handle_reconnection(double delta);
    void _send_led_command(float brightness);
    void _update_fps_counter(double delta);
};

} // namespace godot
