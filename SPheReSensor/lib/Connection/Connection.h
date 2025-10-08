#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "Arduino.h"
#include "Credentials.h" // comment this line
// #include "YourCredentials.h"// uncomment this line
#include "system.h"

class ConnectionHandler {
public:
    bool connection_status = false;
    bool setup();
    void sendData(float moisture, bool valve_state);
    void sendImage(float moisture, camera_fb_t* fb);
    void sendImageBuffer(float moisture, const uint8_t* data, size_t len); //teste 
    String receiveData();
    void logEvent(const String& stage, int code, int bytes, float moisture, const String& msg);
    void close();
private:
    const int timeout = 10000; // 10s
};
extern ConnectionHandler Connection;