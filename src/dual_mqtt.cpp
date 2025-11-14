#include "dual_mqtt.h"

// Global instance pointer for static callback
DualMQTT* g_dual_mqtt = nullptr;

DualMQTT::DualMQTT() :
    _cloud_mqtt(_cloud_client),
    _dev_mqtt(_dev_client),
    _last_cloud_reconnect(0),
    _last_dev_reconnect(0),
    _last_stream_publish(0),
    _stream_counter(0),
    _user_callback(nullptr)
{
    g_dual_mqtt = this;
}

bool DualMQTT::begin(const char* node_id, const char* username, const char* password) {
    _node_id = String(node_id);
    _username = String(username ? username : "");
    _password = String(password ? password : "");

    // Configure cloud MQTT (TLS)
    _cloud_mqtt.setServer(MQTT_PRIMARY_BROKER, MQTT_PRIMARY_PORT);
    _cloud_mqtt.setCallback(cloudCallback);
    _cloud_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    _cloud_mqtt.setBufferSize(16384);  // Large buffer for waveforms

    // Configure TLS
    _cloud_client.setInsecure();  // Skip cert verification (for simplicity)

    // Configure dev MQTT (no TLS)
    #if ENABLE_DEV_BROKER
    _dev_mqtt.setServer(MQTT_DEV_BROKER, MQTT_DEV_PORT);
    _dev_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    _dev_mqtt.setBufferSize(512);  // Smaller buffer for streaming
    #endif

    // Connect to cloud broker
    if (connectCloud()) {
        DEBUG_PRINTLN("[MQTT] ✓ Cloud broker connected");
    } else {
        DEBUG_PRINTLN("[MQTT] ✗ Cloud broker connection failed");
        return false;
    }

    // Connect to dev broker (non-blocking, optional)
    #if ENABLE_DEV_BROKER
    if (connectDev()) {
        DEBUG_PRINTLN("[MQTT] ✓ Dev broker connected");
    } else {
        DEBUG_PRINTLN("[MQTT] ⚠ Dev broker not available (continuing anyway)");
    }
    #endif

    return true;
}

bool DualMQTT::connectCloud() {
    DEBUG_PRINTLN("[MQTT] Connecting to cloud broker...");

    String client_id = "eew_node_" + _node_id;

    bool connected = false;
    if (_username.length() > 0 && _password.length() > 0) {
        connected = _cloud_mqtt.connect(
            client_id.c_str(),
            _username.c_str(),
            _password.c_str()
        );
    } else {
        connected = _cloud_mqtt.connect(client_id.c_str());
    }

    if (connected) {
        DEBUG_PRINTLN("[MQTT] ✓ Cloud connected");
        _last_cloud_reconnect = millis();
        return true;
    }

    DEBUG_PRINTF("[MQTT] ✗ Cloud connection failed, rc=%d\n", _cloud_mqtt.state());
    return false;
}

bool DualMQTT::connectDev() {
    #if ENABLE_DEV_BROKER
    DEBUG_PRINTLN("[MQTT] Connecting to dev broker...");

    String client_id = "eew_dev_" + _node_id;

    if (_dev_mqtt.connect(client_id.c_str())) {
        DEBUG_PRINTLN("[MQTT] ✓ Dev connected");
        _last_dev_reconnect = millis();
        return true;
    }

    DEBUG_PRINTF("[MQTT] ✗ Dev connection failed, rc=%d\n", _dev_mqtt.state());
    #endif

    return false;
}

void DualMQTT::loop() {
    // Handle cloud MQTT
    if (_cloud_mqtt.connected()) {
        _cloud_mqtt.loop();
    } else {
        // Reconnect cloud with backoff
        if (millis() - _last_cloud_reconnect > MQTT_RECONNECT_DELAY) {
            DEBUG_PRINTLN("[MQTT] Reconnecting to cloud...");
            connectCloud();
        }
    }

    // Handle dev MQTT
    #if ENABLE_DEV_BROKER
    if (_dev_mqtt.connected()) {
        _dev_mqtt.loop();
    } else {
        // Reconnect dev with backoff (less critical)
        if (millis() - _last_dev_reconnect > (MQTT_RECONNECT_DELAY * 2)) {
            connectDev();
        }
    }
    #endif
}

bool DualMQTT::publishRegistration(const char* node_id, const char* hardware_id, const char* version) {
    StaticJsonDocument<256> doc;
    doc["node_id"] = node_id;
    doc["hardware_id"] = hardware_id;
    doc["version"] = version;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    // Publish to cloud only
    bool result = _cloud_mqtt.publish(TOPIC_REGISTER, payload.c_str(), false);

    #if ENABLE_DEV_BROKER
    if (_dev_mqtt.connected()) {
        _dev_mqtt.publish(TOPIC_REGISTER, payload.c_str(), false);
    }
    #endif

    return result;
}

bool DualMQTT::publishHeartbeat(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_HEARTBEAT) + node_id;
    String payload;
    serializeJson(data, payload);

    // Publish to cloud
    bool result = _cloud_mqtt.publish(topic.c_str(), payload.c_str(), false);

    // Publish to dev
    #if ENABLE_DEV_BROKER
    if (_dev_mqtt.connected()) {
        _dev_mqtt.publish(topic.c_str(), payload.c_str(), false);
    }
    #endif

    return result;
}

bool DualMQTT::publishTrigger(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_RAW) + node_id;
    String payload;
    serializeJson(data, payload);

    // Publish to cloud
    bool result = _cloud_mqtt.publish(topic.c_str(), payload.c_str(), false);

    // Publish to dev
    #if ENABLE_DEV_BROKER
    if (_dev_mqtt.connected()) {
        _dev_mqtt.publish(topic.c_str(), payload.c_str(), false);
    }
    #endif

    return result;
}

bool DualMQTT::publishStatus(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_STATUS) + node_id;
    String payload;
    serializeJson(data, payload);

    // Publish to cloud
    bool result = _cloud_mqtt.publish(topic.c_str(), payload.c_str(), false);

    // Publish to dev
    #if ENABLE_DEV_BROKER
    if (_dev_mqtt.connected()) {
        _dev_mqtt.publish(topic.c_str(), payload.c_str(), false);
    }
    #endif

    return result;
}

void DualMQTT::publishStreamSample(float x_mg, float y_mg, float z_mg) {
    #if ENABLE_DEV_BROKER
    // Only publish to dev broker (not cloud)
    if (!_dev_mqtt.connected()) {
        return;
    }

    // Rate limiting: Publish every 10th sample (200 Hz → 20 Hz)
    // This reduces MQTT load while keeping visual updates smooth
    _stream_counter++;
    if (_stream_counter < 10) {
        return;
    }
    _stream_counter = 0;

    // Throttle to max 20 Hz (50ms interval)
    if (millis() - _last_stream_publish < 50) {
        return;
    }
    _last_stream_publish = millis();

    // Create compact JSON
    StaticJsonDocument<128> doc;
    doc["x"] = round(x_mg * 10.0) / 10.0;  // 1 decimal place
    doc["y"] = round(y_mg * 10.0) / 10.0;
    doc["z"] = round(z_mg * 10.0) / 10.0;
    doc["timestamp_ms"] = millis();

    String payload;
    serializeJson(doc, payload);

    String topic = String(TOPIC_STREAM) + _node_id;
    _dev_mqtt.publish(topic.c_str(), payload.c_str(), 0);  // QoS 0 for speed
    #endif
}

bool DualMQTT::subscribe(const char* topic) {
    // Only subscribe to cloud commands (dev is one-way publish)
    return _cloud_mqtt.subscribe(topic, MQTT_QOS);
}

bool DualMQTT::isConnected() {
    return _cloud_mqtt.connected();
}

void DualMQTT::setCallback(void (*callback)(char*, byte*, unsigned int)) {
    _user_callback = callback;
}

void DualMQTT::cloudCallback(char* topic, byte* payload, unsigned int length) {
    if (g_dual_mqtt && g_dual_mqtt->_user_callback) {
        g_dual_mqtt->_user_callback(topic, payload, length);
    }
}
