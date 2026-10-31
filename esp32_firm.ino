
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

// Enter your credentials locally. Do not share passwords.
#define WIFI_SSID       "OPPOReno"
#define WIFI_PASSWORD   "55555555"
#define API_KEY         "AIzaSyDNs6nRbb8dZ9ig4CigAxpXo4XYgRvQb6A"
#define DATABASE_URL    "https://edgeguard-1f55c-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL      "akashkash0550@gmail.com"
#define USER_PASSWORD   "edgegurad"

HardwareSerial STM32Serial(2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL = 1000;

void processData(const String &line) {
  StaticJsonDocument<256> input;
  DeserializationError error = deserializeJson(input, line);

  if (error) {
    Serial.println("Invalid JSON from STM32");
    return;
  }

  if (!input.containsKey("frequency_hz") ||
      !input.containsKey("fft_magnitude") ||
      !input.containsKey("temp1_c") ||
      !input.containsKey("temp2_c")) {
    Serial.println("Sensor fields missing");
    return;
  }

  float frequency = input["frequency_hz"].as<float>();
  float magnitude = input["fft_magnitude"].as<float>();
  float temp1 = input["temp1_c"].as<float>();
  float temp2 = input["temp2_c"].as<float>();

  Serial.printf(
    "Received: Freq=%.2f Hz, Magnitude=%.2f, T1=%.2f C, T2=%.2f C\n",
    frequency, magnitude, temp1, temp2
  );

  if (millis() - lastUpload < UPLOAD_INTERVAL) return;
  if (!Firebase.ready()) {
    Serial.println("Firebase not ready yet");
    return;
  }

  FirebaseJson json;
  json.set("frequency_hz", frequency);
  json.set("fft_magnitude", magnitude);
  json.set("temp1_c", temp1);
  json.set("temp2_c", temp2);
  json.set("updated_ms", (int)millis());

  Serial.print("Firebase upload: ");

  if (Firebase.RTDB.setJSON(&fbdo, "/edgeguard/live", &json)) {
    Serial.println("SUCCESS");
    lastUpload = millis();
  } else {
    Serial.print("FAILED: ");
    Serial.println(fbdo.errorReason());
  }
}

void setup() {
  Serial.begin(115200);

  // STM32 TX -> ESP32 GPIO16 (RX)
  // STM32 RX <- ESP32 GPIO17 (TX)
  STM32Serial.begin(115200, SERIAL_8N1, 16, 17);

  Serial.println("\nEdgeGuard starting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to hotspot");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nWi-Fi connected");
  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  Serial.println("Firebase initialization started");
}

void loop() {
  // Continue Firebase authentication and token processing.
  // Firebase.ready() is checked before each write.
  static String line;

  while (STM32Serial.available()) {
    char c = (char)STM32Serial.read();

    if (c == '\n') {
      line.trim();
      if (line.length() > 0) processData(line);
      line = "";
    } else if (line.length() < 250) {
      line += c;
    } else {
      line = "";
      Serial.println("UART line too long; discarded");
    }
  }
}
