#include "mqtt_handler.h"

// HiveMQ Cloud Root CA Certificate
// HiveMQ Cloud CA - Let's Encrypt ISRG Root X1
const char* hivemq_root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

MQTTHandler* g_mqtt_handler = nullptr;

MQTTHandler::MQTTHandler() : _mqtt_client(_wifi_client) {
    _last_reconnect_attempt = 0;
    _user_callback = nullptr;
    g_mqtt_handler = this;
}

bool MQTTHandler::begin(const char* node_id, const char* username, const char* password) {
    _node_id = String(node_id);

    // Store credentials
    if (username != nullptr) {
        _mqtt_username = String(username);
    }
    if (password != nullptr) {
        _mqtt_password = String(password);
    }

    // Check if credentials are provided
    if (_mqtt_username.length() == 0 || _mqtt_password.length() == 0) {
        DEBUG_PRINTLN("[MQTT] ✗ ERROR: No MQTT credentials provided!");
        DEBUG_PRINTLN("[MQTT] Please configure via WiFi portal on next boot");
        return false;
    }

    // Configure TLS
    _wifi_client.setCACert(hivemq_root_ca);

    // Configure MQTT
    _mqtt_client.setServer(MQTT_PRIMARY_BROKER, MQTT_PRIMARY_PORT);
    _mqtt_client.setCallback(staticCallback);
    _mqtt_client.setKeepAlive(MQTT_KEEPALIVE_S);
    _mqtt_client.setBufferSize(8192);  // Larger buffer for waveform data

    DEBUG_PRINTLN("[MQTT] Configured");

    return connectToMQTT();
}

bool MQTTHandler::connectToMQTT() {
    if (_mqtt_client.connected()) {
        return true;
    }
    
    DEBUG_PRINT("[MQTT] Connecting to ");
    DEBUG_PRINT(MQTT_PRIMARY_BROKER);
    DEBUG_PRINT(":");
    DEBUG_PRINTLN(MQTT_PRIMARY_PORT);
    
    // Attempt connection
    bool connected = _mqtt_client.connect(
        _node_id.c_str(),
        _mqtt_username.c_str(),
        _mqtt_password.c_str()
    );
    
    if (connected) {
        DEBUG_PRINTLN("[MQTT] ✓ Connected!");
        
        // Subscribe to command topic
        String cmd_topic = String(TOPIC_CMD) + _node_id;
        subscribe(cmd_topic.c_str());
        
        // Subscribe to alert topic
        subscribe(TOPIC_ALERT);
        
        return true;
    } else {
        DEBUG_PRINT("[MQTT] ✗ Failed, rc=");
        DEBUG_PRINTLN(_mqtt_client.state());
        return false;
    }
}

void MQTTHandler::loop() {
    if (!_mqtt_client.connected()) {
        unsigned long now = millis();
        if (now - _last_reconnect_attempt > MQTT_RECONNECT_DELAY) {
            _last_reconnect_attempt = now;
            DEBUG_PRINTLN("[MQTT] Reconnecting...");
            connectToMQTT();
        }
    } else {
        _mqtt_client.loop();
    }
}

bool MQTTHandler::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqtt_client.connected()) {
        DEBUG_PRINTLN("[MQTT] Cannot publish - not connected");
        return false;
    }
    
    bool result = _mqtt_client.publish(topic, payload, retained);
    
    #if DEBUG_MQTT
    if (result) {
        DEBUG_PRINT("[MQTT] ✓ Published to ");
        DEBUG_PRINTLN(topic);
    } else {
        DEBUG_PRINT("[MQTT] ✗ Publish failed to ");
        DEBUG_PRINTLN(topic);
    }
    #endif
    
    return result;
}

bool MQTTHandler::publish(const char* topic, JsonDocument& doc, bool retained) {
    String payload;
    serializeJson(doc, payload);
    return publish(topic, payload.c_str(), retained);
}

bool MQTTHandler::subscribe(const char* topic) {
    bool result = _mqtt_client.subscribe(topic, MQTT_QOS);
    
    #if DEBUG_MQTT
    if (result) {
        DEBUG_PRINT("[MQTT] ✓ Subscribed to ");
        DEBUG_PRINTLN(topic);
    } else {
        DEBUG_PRINT("[MQTT] ✗ Subscribe failed to ");
        DEBUG_PRINTLN(topic);
    }
    #endif
    
    return result;
}

bool MQTTHandler::isConnected() {
    return _mqtt_client.connected();
}

void MQTTHandler::disconnect() {
    _mqtt_client.disconnect();
}

void MQTTHandler::setCallback(MQTTCallback callback) {
    _user_callback = callback;
}

void MQTTHandler::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (g_mqtt_handler && g_mqtt_handler->_user_callback) {
        g_mqtt_handler->_user_callback(topic, payload, length);
    }
}

// ============================================================================
// Helper Publishing Methods
// ============================================================================

bool MQTTHandler::publishRegistration(const char* node_id, const char* hardware_id, const char* version) {
    StaticJsonDocument<512> doc;
    
    doc["hardware_id"] = hardware_id;
    doc["node_id"] = node_id;
    doc["version"] = version;
    doc["chip_model"] = ESP.getChipModel();
    doc["timestamp_us"] = (uint64_t)esp_timer_get_time();
    doc["sequence"] = 0;
    
    return publish(TOPIC_REGISTER, doc);
}

bool MQTTHandler::publishHeartbeat(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_HEARTBEAT) + node_id;
    return publish(topic.c_str(), data);
}

bool MQTTHandler::publishTrigger(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_RAW) + node_id;
    return publish(topic.c_str(), data);
}

bool MQTTHandler::publishStatus(const char* node_id, JsonDocument& data) {
    String topic = String(TOPIC_STATUS) + node_id;
    return publish(topic.c_str(), data);
}