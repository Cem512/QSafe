#ifndef DUAL_MQTT_H
#define DUAL_MQTT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

/**
 * Dual MQTT Handler - Publishes to both cloud (HiveMQ) and local dev broker
 *
 * Primary (Cloud):
 *   - HiveMQ Cloud with TLS
 *   - Triggers, heartbeats, status only
 *   - Username/password authentication
 *
 * Development (Local):
 *   - Raspberry Pi Mosquitto (no TLS)
 *   - All data including 200 Hz streaming
 *   - No authentication (local network only)
 */

class DualMQTT {
public:
    DualMQTT();

    bool begin(const char* node_id, const char* username = nullptr, const char* password = nullptr);
    void loop();

    // Publishing methods
    bool publishRegistration(const char* node_id, const char* hardware_id, const char* version);
    bool publishHeartbeat(const char* node_id, JsonDocument& data);
    bool publishTrigger(const char* node_id, JsonDocument& data);
    bool publishStatus(const char* node_id, JsonDocument& data);

    // Dev-only: Stream raw acceleration data (200 Hz)
    void publishStreamSample(float x_mg, float y_mg, float z_mg);

    // Subscription (only subscribe to cloud commands)
    bool subscribe(const char* topic);

    bool isConnected();
    void setCallback(void (*callback)(char*, byte*, unsigned int));

private:
    // Cloud broker (TLS)
    WiFiClientSecure _cloud_client;
    PubSubClient _cloud_mqtt;

    // Dev broker (no TLS)
    WiFiClient _dev_client;
    PubSubClient _dev_mqtt;

    String _node_id;
    String _username;
    String _password;

    unsigned long _last_cloud_reconnect;
    unsigned long _last_dev_reconnect;
    unsigned long _last_stream_publish;

    // Streaming rate limiter (publish every Nth sample)
    uint16_t _stream_counter;

    bool connectCloud();
    bool connectDev();

    static void cloudCallback(char* topic, byte* payload, unsigned int length);
    void (*_user_callback)(char*, byte*, unsigned int);
};

extern DualMQTT* g_dual_mqtt;

#endif // DUAL_MQTT_H
