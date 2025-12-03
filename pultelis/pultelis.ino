#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include "secrets.h"
#include "esp_task_wdt.h"

const int SERVO_PIN = 13;
const int SERVO_DEFAULT_POSITION = 20; // degrees
const int SERVO_PRESS_POSITION = 15; // degrees
const int SERVO_PRESS_DURATION = 500; // ms
const int WATCHDOG_TIMEOUT = 120; // seconds

Servo servo;

void send_message(String message) {
    HTTPClient http;
    http.begin(String("http://ntfy.sh/") + NTFY_DEBUG_TOPIC);
    http.addHeader("Content-Type", "text/plain");
    http.POST(message);
    http.end();
}

void log_message(String message) {
    Serial.println(message);
    // if (Serial.availableForWrite()) {
    //     Serial.println(message);
    // } else {
    //     send_message(message);
    // }
}

void press_remote() {
    servo.write(SERVO_PRESS_POSITION);
    delay(SERVO_PRESS_DURATION);
    
    servo.write(SERVO_DEFAULT_POSITION);
    delay(200);
}

void setup() {
    Serial.begin(9600);
    Serial.println("\nConnecting to Wi-Fi...");

    servo.attach(SERVO_PIN);
    servo.write(SERVO_DEFAULT_POSITION);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
    log_message("\nWi-Fi connected!");
    
    // Reconfigure hardware watchdog timer
    // Arduino framework already initializes it, so we reconfigure instead
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT * 1000,
        .idle_core_mask = 0,  // Don't check idle cores
        .trigger_panic = true  // Trigger panic (restart) on timeout
    };
    
    esp_err_t err = esp_task_wdt_reconfigure(&wdt_config);
    if (err != ESP_OK) {
        log_message("Failed to reconfigure watchdog timer");
    }
    
    // Add current task to watchdog (if not already added)
    err = esp_task_wdt_add(NULL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        log_message("Failed to add task to watchdog");
    }
    send_message("Initialized!");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        log_message("[HTTP] Beginning NTFY request...\n");
        http.begin(String("http://ntfy.sh/") + NTFY_TOPIC + "/raw");

        int httpCode = http.GET();

        if (httpCode > 0) {
            String responseMsg = "[HTTP] Response code: " + String(httpCode) + "\n";
            log_message(responseMsg);

            if (httpCode == HTTP_CODE_OK) {
                NetworkClient& stream = http.getStream();
                
                log_message("Streaming NTFY response data...");
                uint8_t buffer[128] = {0};

                while (stream.connected()) {
                    size_t available = stream.available();
                    size_t toRead = min(available, sizeof(buffer));
                    size_t size = stream.read(buffer, toRead);
                    if (size > 0) {
                        // Reset hardware watchdog timer
                        esp_task_wdt_reset();
                        
                        String chunk = String((char*)buffer).substring(0, size);
                        // Check for all occurrences of KEY_STRING in this chunk and trigger for each
                        int keyIndex = chunk.indexOf(KEY_STRING);
                        if (keyIndex < 0) {
                            log_message("No key string found in chunk: " + chunk);
                        }
                        while (keyIndex >= 0) {
                            log_message("\n[INFO] Key string found! Calling press_remote()");
                            press_remote();
                            send_message("Remote pressed");
                            // Remove the portion up to and including the KEY_STRING
                            // Keep the rest of the chunk for potential future matches in same chunk
                            chunk = chunk.substring(keyIndex + strlen(KEY_STRING));
                            keyIndex = chunk.indexOf(KEY_STRING);
                        }
                    }
                    delay(100);
                }
                log_message("\nConnection closed by server");
            }
        } else {
            String errorMsg = "[HTTP] GET failed, error: " + String(HTTPClient::errorToString(httpCode).c_str()) + "\n";
            log_message(errorMsg);
        }
        http.end();
        
        // Small delay before sending HTTP request again
        delay(1000);
    }
    delay(100);
}