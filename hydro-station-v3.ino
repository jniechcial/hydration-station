// Pseudo-code for ESP32 plant hydration system

// Include necessary libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Log entry structure
struct LogEntry {
    String message;
    unsigned long timestamp;
};

// Constants and thresholds
const char* ssid = "ASUS D";
const char* password = "kubakubakuba";
const char* cloudEndpoint = "https://script.google.com/macros/s/AKfycbxwAn0VSgGbhCpxRON-2UIGyGJd7v7Twkp7hWlsWLH-LOPjedzrv18uvsrY1vtiTfUD/exec";

const int MAX_LOGS = 100;  // Maximum number of logs to store
LogEntry logCache[MAX_LOGS];
int logCount = 0;

const int moistureThresholdPlant1 = 1750;  // Define experimentally
const int moistureThresholdPlant2 = 1400;

// PWM properties
const int freq = 5000;
const int pumpGateChannel = 0;
const int resolution = 8;

const int pumpDutyCycle = 185;
const int singlePlantWateringTime = 2500;

const int pumpGatePin = 17;
const int moisturePin = 15;
const int valvePin = 5;

const int valvePlant1Pin = 5; // GPIO pin for valve 1
const int valvePlant2Pin = 18; // GPIO pin for valve 2
const int moistureSensor1Pin = 33; // GPIO pin for moisture sensor 1
const int moistureSensor2Pin = 32; // GPIO pin for moisture sensor 2

const unsigned long checkInterval = 5 * 60 * 1000; // 5 minutes

unsigned long lastCheckTime = 0;

void setup() {
    // Initialize serial, WiFi, sensors, valves, and pump
    Serial.begin(115200);

    // Initialize valves and pump
    pinMode(valvePlant1Pin, OUTPUT);
    pinMode(valvePlant2Pin, OUTPUT);
    digitalWrite(valvePlant1Pin, LOW);
    digitalWrite(valvePlant2Pin, LOW);

    ledcAttachChannel(pumpGatePin, freq, resolution, pumpGateChannel);
}

void loop() {
    unsigned long currentTime = millis();

    if (currentTime - lastCheckTime >= checkInterval) {
        lastCheckTime = currentTime;

        WiFi.begin(ssid, password);
        Serial.printf("Starting to connect to wifi... \n");
        int tries = 0;
        while ((WiFi.status() != WL_CONNECTED) && (tries < 100)) {
            Serial.printf("WiFi connection attempt: %d \n", tries);
            tries++;
            delay(500);
        }

        bool WiFiStatus = WiFi.status() == WL_CONNECTED;

        // Read moisture levels
        int moisturePlant1 = analogRead(moistureSensor1Pin);
        int moisturePlant2 = analogRead(moistureSensor2Pin);

        logToSerialAndCacheCloud(
          String("Plant 1 moisture: ") + moisturePlant1
        );

        logToSerialAndCacheCloud(
          String("Plant 2 moisture: ") + moisturePlant2
        );

        // Determine if watering is needed
        bool waterPlant1 = moisturePlant1 < moistureThresholdPlant1;
        bool waterPlant2 = moisturePlant2 < moistureThresholdPlant2;

        if (waterPlant1 || waterPlant2) {
            // Activate valves as needed
            if (waterPlant1) {
                digitalWrite(valvePlant1Pin, HIGH);
                logToSerialAndCacheCloud(
                  String("Valve for Plant 1 opened")
                );
            }
            if (waterPlant2) {
                digitalWrite(valvePlant2Pin, HIGH);
                logToSerialAndCacheCloud(
                  String("Valve for Plant 2 opened")
                );
            }

            // Start pump                        
            ledcWrite(pumpGatePin, pumpDutyCycle);

            logToSerialAndCacheCloud(
              String("Pump started")
            );

            // Delay to allow watering
            int valvesOpened = (int)waterPlant1 + (int)waterPlant2;
            delay(singlePlantWateringTime * valvesOpened);  

            // Stop pump and close valves
            ledcWrite(pumpGatePin, 0);
            logToSerialAndCacheCloud(
              String("Pump stopped")
            );

            digitalWrite(valvePlant1Pin, LOW);
            digitalWrite(valvePlant2Pin, LOW);
            logToSerialAndCacheCloud(
              String("All valves closed")
            );
        }

        flushCloudCache(WiFiStatus);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
}

void logToSerialAndCacheCloud(String log) {
    Serial.println(log);

    // Add to cache if there's space
    if (logCount < MAX_LOGS) {
        logCache[logCount].message = log;
        logCache[logCount].timestamp = millis();
        logCount++;
    } else {
        Serial.println("Warning: Log cache full, dropping message");
    }
}

void flushCloudCache(bool wifiStatus) {
    if (wifiStatus) {
        HTTPClient http;

        // Create a JSON array from the logs
        DynamicJsonDocument jsonDoc(2048); // Increased size for timestamps
        JsonArray jsonArray = jsonDoc.to<JsonArray>();

        for (int i = 0; i < logCount; i++) {
            JsonObject logObject = jsonArray.createNestedObject();
            logObject["message"] = logCache[i].message;
            logObject["timestamp"] = logCache[i].timestamp;
        }

        // Serialize JSON to a string
        String jsonString;
        serializeJson(jsonDoc, jsonString);

        // Begin the HTTP connection
        http.begin(cloudEndpoint);
        http.addHeader("Content-Type", "application/json");

        // Perform the POST request with the JSON payload
        int httpResponseCode = http.POST(jsonString);

        // Handle the response
        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.print("Error on sending HTTP request: ");
            Serial.println(http.errorToString(httpResponseCode));
        }

        // End the connection
        http.end();

        logCount = 0;
    }    
}
