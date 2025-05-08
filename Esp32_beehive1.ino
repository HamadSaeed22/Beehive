#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "RIT-STUDENT";  // Your WiFi SSID
const char* password = "RitStd@123";   // Your WiFi Password

String webAppUrl = "https://script.google.com/macros/s/AKfycbzP1z94-cIR6UZZneeSpoHRNiNP-jrk_BQ3T_Ltj7bLQ3mBtOnpOzggdidJL2uC92GxGQ/exec";  // Your Google Apps Script URL

void setup() {
  Serial.begin(115200);  // Start Serial for ESP32
  Serial2.begin(115200,SERIAL_8N1, 16, 17); // Start Serial2 for communication with Arduino (Use correct RX/TX pins)

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void loop() {
  if (Serial2.available()) {  // Check if data is available from Arduino
    String data = Serial2.readStringUntil('\n'); // Read full line of CSV data

    data.trim(); // Remove unwanted spaces or newline characters

    if (data.length() > 0) {
      Serial.println("Received: " + data);
      sendDataToWebApp(data);
    }
  }

  delay(1000);  // Adjust delay if needed
}

void sendDataToWebApp(String data) {
  HTTPClient http;
  http.begin(webAppUrl);  
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String requestBody = data;


  int httpResponse = http.POST(requestBody);

  if (httpResponse == 200) {
    Serial.println("Data sent successfully!");
  } else {
    Serial.print("Error sending data: ");
    Serial.println(httpResponse);
    Serial.println("Response: " + http.getString());
  }

  http.end();
}