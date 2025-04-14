#include <WiFiS3.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

// WiFi credentials
const char* ssid = "Saeed-ALAli-3";
const char* password = "Ses325_S";

// Google Apps Script Web App URL
String webAppUrl = "https://script.google.com/macros/s/AKfycbzP1z94-cIR6UZZneeSpoHRNiNP-jrk_BQ3T_Ltj7bLQ3mBtOnpOzggdidJL2uC92GxGQ/exec";

// DHT Pins & Type
#define DHTTYPE DHT22
#define AMBIENT_DHTPIN 4
#define DHTPIN2 5
#define DHTPIN3 6

DHT ambientDHT(AMBIENT_DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);

// DS18B20
#define ONE_WIRE_BUS 10
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Relays
#define RELAY_PUMP 11
#define RELAY_FAN1 12
#define RELAY_FAN2 13

// Servo
#define SERVO_PIN 7
Servo flapServo;

// Readings buffer
const int numReadings = 5;
float dhtReadings[2][numReadings] = {0};
float dsReadings[6][numReadings] = {0};
int dhtIndex = 0, dsIndex = 0;

void setup() {
  Serial.begin(115200);

  // Sensor init
  ambientDHT.begin();
  dht2.begin();
  dht3.begin();
  ds18b20.begin();

  // Relay output
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_FAN1, OUTPUT);
  pinMode(RELAY_FAN2, OUTPUT);
  digitalWrite(RELAY_PUMP, HIGH);
  digitalWrite(RELAY_FAN1, HIGH);
  digitalWrite(RELAY_FAN2, HIGH);

  // Servo
  flapServo.attach(SERVO_PIN);
  flapServo.write(180); // Closed

  // Connect WiFi
  Serial.print("Connecting to WiFi...");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void loop() {
  float tempAmbient = ambientDHT.readTemperature();
  float newDHT2 = dht2.readTemperature();
  float newDHT3 = dht3.readTemperature();

  float newDS[6];
  ds18b20.requestTemperatures();
  for (int i = 0; i < 6; i++) {
    newDS[i] = ds18b20.getTempCByIndex(i);
  }

  dhtReadings[0][dhtIndex] = !isnan(newDHT2) ? newDHT2 : dhtReadings[0][dhtIndex];
  dhtReadings[1][dhtIndex] = !isnan(newDHT3) ? newDHT3 : dhtReadings[1][dhtIndex];

  for (int i = 0; i < 6; i++) {
    dsReadings[i][dsIndex] = !isnan(newDS[i]) ? newDS[i] : dsReadings[i][dsIndex];
  }

  dhtIndex = (dhtIndex + 1) % numReadings;
  dsIndex = (dsIndex + 1) % numReadings;

  float avgDHT[2] = {0};
  float avgDS[6] = {0};
  int countDHT[2] = {0};
  int countDS[6] = {0};

  for (int i = 0; i < numReadings; i++) {
    for (int j = 0; j < 2; j++) {
      if (!isnan(dhtReadings[j][i])) {
        avgDHT[j] += dhtReadings[j][i];
        countDHT[j]++;
      }
    }
    for (int j = 0; j < 6; j++) {
      if (!isnan(dsReadings[j][i])) {
        avgDS[j] += dsReadings[j][i];
        countDS[j]++;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    avgDHT[j] = countDHT[j] > 0 ? avgDHT[j] / countDHT[j] : NAN;
  }
  for (int j = 0; j < 6; j++) {
    avgDS[j] = countDS[j] > 0 ? avgDS[j] / countDS[j] : NAN;
  }

  bool enoughData = (dhtIndex == 0);
  if (enoughData) {
    float allTemps[8];
    float totalAvg = 0;
    int validCount = 0;

    for (int i = 0; i < 2; i++) {
      allTemps[i] = avgDHT[i];
      if (!isnan(avgDHT[i])) {
        totalAvg += avgDHT[i];
        validCount++;
      }
    }

    for (int i = 0; i < 6; i++) {
      allTemps[i + 2] = avgDS[i];
      if (!isnan(avgDS[i])) {
        totalAvg += avgDS[i];
        validCount++;
      }
    }

    float avgTemp = validCount > 0 ? totalAvg / validCount : NAN;

    // Control relays
    digitalWrite(RELAY_FAN1, avgTemp >= 36 ? LOW : HIGH);
    digitalWrite(RELAY_FAN2, avgTemp >= 37.5 ? LOW : HIGH);
    digitalWrite(RELAY_PUMP, avgTemp >= 39 ? LOW : HIGH);

    String pumpState = digitalRead(RELAY_PUMP) == LOW ? "ON" : "OFF";
    String fan1State = digitalRead(RELAY_FAN1) == LOW ? "ON" : "OFF";
    String fan2State = digitalRead(RELAY_FAN2) == LOW ? "ON" : "OFF";

    // Servo logic
    float servoPosition = 180;
    String servoStatus = "Closed";
    if (avgTemp >= 36) { servoPosition = 0; servoStatus = "100% Open"; }
    else if (avgTemp >= 35) { servoPosition = 45; servoStatus = "75% Open"; }
    else if (avgTemp >= 34) { servoPosition = 90; servoStatus = "50% Open"; }
    else if (avgTemp >= 33) { servoPosition = 135; servoStatus = "25% Open"; }
    flapServo.write(servoPosition);

    // Build CSV string
    String csvData = String(tempAmbient) + ",";
    for (int i = 0; i < 8; i++) {
      csvData += String(allTemps[i]) + ",";
    }
    csvData += String(avgTemp) + ",";
    csvData += pumpState + "," + fan1State + "," + fan2State + "," + servoStatus;

    sendDataToWebApp(csvData);
  }

  delay(12000);
}

void sendDataToWebApp(String data) {
  HTTPClient http;

  if (WiFi.status() == WL_CONNECTED) {
    http.begin(webAppUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(data);

    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode != 200) {
      Serial.println("Failed to send data: " + http.getString());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
