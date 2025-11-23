#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include "secrets.h"

const int SERVO_PIN = 13;
const int SERVO_DEFAULT_POSITION = 20; // degrees
const int SERVO_PRESS_POSITION = 17; // degrees
const int SERVO_PRESS_DURATION = 500; // ms

Servo servo;

void press_remote() {
    Serial.println("press_remote() called! Moving servo to 90 degrees...");
    servo.write(SERVO_PRESS_POSITION);
    delay(SERVO_PRESS_DURATION);
    
    servo.write(SERVO_DEFAULT_POSITION);
    delay(200);
    Serial.println("Servo movement complete.");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nConnecting to Wi-Fi...");

    servo.attach(SERVO_PIN);
    servo.write(SERVO_DEFAULT_POSITION);
    Serial.println("Servo initialized on pin " + String(SERVO_PIN));

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected!");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        Serial.print("[HTTP] Beginning request...\n");
        http.begin(String("http://ntfy.sh/") + NTFY_TOPIC + "/raw");

        Serial.print("[HTTP] Sending GET...\n");
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.printf("[HTTP] Response code: %d\n", httpCode);

            if (httpCode == HTTP_CODE_OK) {
                NetworkClient& stream = http.getStream();
                
                Serial.println("Streaming response data:");
                uint8_t buffer[128] = {0};

                while (stream.connected()) {
                    size_t available = stream.available();
                    size_t toRead = min(available, sizeof(buffer));
                    size_t size = stream.read(buffer, toRead);
                    if (size > 0) {
                        Serial.write(buffer, size);

                        String chunk = String((char*)buffer).substring(0, size);
                        // Check for all occurrences of KEY_STRING in this chunk and trigger for each
                        int keyIndex = chunk.indexOf(KEY_STRING);
                        while (keyIndex >= 0) {
                            Serial.println("\n[INFO] Key string found! Calling press_remote()");
                            press_remote();
                            // Remove the portion up to and including the KEY_STRING
                            // Keep the rest of the chunk for potential future matches in same chunk
                            chunk = chunk.substring(keyIndex + strlen(KEY_STRING));
                            keyIndex = chunk.indexOf(KEY_STRING);
                        }
                    }
                }
                Serial.println("\nConnection closed by server");
                Serial.println("\nStreaming complete.");
            }
        } else {
            Serial.printf("[HTTP] GET failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
        }
        http.end();
        
        // Small delay before sending HTTP request again
        delay(1000);
    }
}