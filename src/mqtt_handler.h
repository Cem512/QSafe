#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Forward declaration for callback
class MQTTHandler;
typedef void (*MQTTCallback)(char* topic, byte* payload, unsigned int length);

class MQTTHandler {
public:
    MQTTHandler();
    
    bool begin(const char* node_id, const char* username = nullptr, const char* password = nullptr);
    void loop();
    
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publish(const char* topic, JsonDocument& doc, bool retained = false);
    
    bool subscribe(const char* topic);
    
    bool isConnected();
    void disconnect();
    
    void setCallback(MQTTCallback callback);
    
    // Helper methods
    bool publishRegistration(const char* node_id, const char* hardware_id, const char* version);
    bool publishHeartbeat(const char* node_id, JsonDocument& data);
    bool publishTrigger(const char* node_id, JsonDocument& data);
    bool publishStatus(const char* node_id, JsonDocument& data);
    
private:
    WiFiClientSecure _wifi_client;
    PubSubClient _mqtt_client;

    String _node_id;
    String _mqtt_username;
    String _mqtt_password;
    unsigned long _last_reconnect_attempt;

    bool connectToMQTT();
    static void staticCallback(char* topic, byte* payload, unsigned int length);

    MQTTCallback _user_callback;
};

// Global instance pointer for static callback
extern MQTTHandler* g_mqtt_handler;

#endif // MQTT_HANDLER_H