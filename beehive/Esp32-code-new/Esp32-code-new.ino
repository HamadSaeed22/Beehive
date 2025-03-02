#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Saeed-ALAli-3";  // Your WiFi SSID
const char* password = "Ses325_S";   // Your WiFi Password

String webAppUrl = "https://script.google.com/macros/s/AKfycbzjMCcP0DYF9fO4DYgvBbGblm5h7LMdtssqe9sW9AjER0048rQUZBhDfrGFvwz3A5Fl8Q/exec";  // Your Google Apps Script URL

void setup() {
  Serial.begin(115200);  // Start Serial for ESP32
  Serial2.begin(115200); // Start Serial2 for communication with Arduino (Use correct RX/TX pins)

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
  Serial.println("Sending data: " + data);

  http.begin(webAppUrl);  
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String requestBody = "data=" + data;
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
