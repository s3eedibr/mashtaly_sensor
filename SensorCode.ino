#include <Arduino.h>  // Standard Arduino library
#include <ESP8266WiFi.h>  // ESP8266 WiFi library
#include <FirebaseESP8266.h>  // Firebase library for ESP8266
#include <WiFiManager.h>  // WiFiManager library for easy WiFi configuration
#include <ESP8266HTTPClient.h>  // HTTP client library for ESP8266
#include <ArduinoJson.h>  // ArduinoJson library for JSON parsing

// WiFi credentials
const char* WIFI_SSID = "Mashtaly Sensor";
const char* WIFI_PASSWORD = "Sensor23";

// Firebase credentials
const char* API_KEY = "**-**";
const char* DATABASE_URL = "mashtaly-hu-**-**.**.com";

FirebaseData fbdo;  // Firebase Data object
FirebaseAuth auth;  // Firebase Authentication object
FirebaseConfig config;  // Firebase Configuration object

// RGB LED pins
const int RED_PIN = 4;     // ~D2
const int GREEN_PIN = 14;  // ~D5
const int BLUE_PIN = 12;   // ~D6
unsigned long timeHolder = 0;  // Variable to hold time

// Reset button pin
const int RESET_BUTTON_PIN = 0;
int buttonState = HIGH;      // Current state of the button
int lastButtonState = HIGH;  // Previous state of the button

int chartValue = 0;  // Initialize chartValue

// Declare global variable to store UID
String uid = "";

// Flag to check if UID is already obtained
bool uidObtained = false;

// Function prototypes
void setLEDColor(int red, int green, int blue);
void handleResetButton();
void handleLEDColor();
void saveUID(const String& receivedUID);
WiFiManager wifiManager;  // WiFiManager instance for easy WiFi setup

void setup() {
  Serial.begin(115200);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  wifiManager.autoConnect(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = "**@gmail.com";
  auth.user.password = "**2023";

  Firebase.reconnectWiFi(true);

  fbdo.setBSSLBufferSize(4096, 1024);

  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(4);
}

void loop() {
  chartValue = analogRead(A0) * 100 / 345 - 100;
  handleResetButton();
  handleLEDColor();

  if (Firebase.ready() && !uidObtained) {
    const char* host = "www.dweet.io";
    const char* thing = "Uid";
    const char* thing_content = "uid";
    Serial.print("Connecting to ");
    Serial.println(host);

    WiFiClient client;
    const int httpPort = 80;

    const int maxRetries = 3;
    int retryCount = 0;

    while (!client.connect(host, httpPort) && retryCount < maxRetries) {
      delay(4000);  // Wait for 4 seconds before retrying
      retryCount++;
    }

    if (retryCount >= maxRetries) {
      Serial.println("Failed to connect to Dweet after multiple attempts");
      return;
    }

    // Prepare URL for Dweet.io request
    String url = "/get/latest/dweet/for/";
    url += thing;

    Serial.print("Requesting URL: ");
    Serial.println(url);

    String getData = thing_content;
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Content-Type: application/x-www-form-urlencoded\r\n" + "Content-Length: " + getData.length() + "\r\n" + "Connection: close\r\n\r\n" + getData);

    int timeout = millis() + 5000;  // Increase timeout to 5000 milliseconds (5 seconds)
    String line;
    while (client.available() == 0) {
      if (millis() > timeout) {
        Serial.println("Client Timeout");
        client.stop();
        return;
      }
    }
    while (client.available()) {
      line = client.readStringUntil('\r');
      Serial.print(line);
    }

    // Parse JSON response
    const size_t bufferSize = JSON_OBJECT_SIZE(4) + 250;
    DynamicJsonDocument jsonBuffer(bufferSize);

    DeserializationError error = deserializeJson(jsonBuffer, line);
    if (error) {
      Serial.println("Error parsing JSON");
    } else {
      // Convert JSON value to String
      String receivedUID = jsonBuffer["with"][0]["content"]["uid"].as<String>();
      Serial.print("Received UID from Dweet: ");
      Serial.println(receivedUID);

      // Save UID for later use
      saveUID(receivedUID);
      uidObtained = true;
    }
    client.stop();
  }

  // If UID is obtained, use it to send data to Firebase
  if (uidObtained) {
    if (Firebase.setInt(fbdo, "/users/" + uid + "/measurement", chartValue)) {
      Serial.println("Data sent to Firebase successfully");
    } else {
      Serial.print("Error sending data to Firebase: ");
      Serial.println(fbdo.errorReason());
    }

    delay(5000);
    setLEDColor(0, 0, 0);
  }
}

void setLEDColor(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void handleResetButton() {
  buttonState = digitalRead(RESET_BUTTON_PIN);
  if (buttonState == LOW && lastButtonState == HIGH) {
    wifiManager.resetSettings();
    Serial.println("WiFi settings reset.");
    ESP.restart();
  }
}

void handleLEDColor() {
  if (millis() - 5000 > timeHolder) {
    timeHolder = millis();

    if (chartValue <= 25) {
      setLEDColor(0, 255, 0);  // Green
    } else if (chartValue > 25 && chartValue <= 75) {
      setLEDColor(255, 255, 0);  // Yellow
    } else if (chartValue > 75) {
      setLEDColor(255, 0, 0);  // Red "dry"
    }

    delay(1000);
    setLEDColor(0, 0, 0);
  }
}

// Function to save UID for later use
void saveUID(const String& receivedUID) {
  // Check if the received UID is not empty
  if (!receivedUID.isEmpty()) {
    uid = receivedUID;
    Serial.print("Saved UID for later use: ");
    Serial.println(uid);
  } else {
    Serial.println("Received UID from Dweet is empty/null. Not saving.");
  }
}