#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

// WiFi credentials
const char* ssid = "RIT-STUDENT";
const char* password = "RitStd@123";

// Google Apps Script URL
String webAppUrl = "https://script.google.com/macros/s/AKfycbzjMCcP0DYF9fO4DYgvBbGblm5h7LMdtssqe9sW9AjER0048rQUZBhDfrGFvwz3A5Fl8Q/exec";

// DHT sensor pins
#define AMBIENT_DHTPIN 4
#define DHTPIN2 5
#define DHTPIN3 6

// DS18B20 pin
#define ONE_WIRE_BUS 10

// Relay pins
#define RELAY_PUMP 11
#define RELAY_FAN1 12
#define RELAY_FAN2 13

// Servo pin
#define SERVO_PIN 7

#define DHTTYPE DHT22

DHT ambientDHT(AMBIENT_DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Servo flapServo;

const int numReadings = 5;
float dhtReadings[2][numReadings] = {0};
float dsReadings[6][numReadings] = {0};
int dhtIndex = 0, dsIndex = 0;

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Sensor setup
  ambientDHT.begin();
  dht2.begin();
  dht3.begin();
  ds18b20.begin();

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_FAN1, OUTPUT);
  pinMode(RELAY_FAN2, OUTPUT);

  digitalWrite(RELAY_PUMP, HIGH);
  digitalWrite(RELAY_FAN1, HIGH);
  digitalWrite(RELAY_FAN2, HIGH);

  flapServo.attach(SERVO_PIN);
  flapServo.write(180); // Closed position
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

  float avgDHT[2] = {0}, avgDS[6] = {0};
  int countDHT[2] = {0}, countDS[6] = {0};

  for (int i = 0; i < numReadings; i++) {
    for (int j = 0; j < 2; j++) {
      if (!isnan(dhtReadings[j][i])) { avgDHT[j] += dhtReadings[j][i]; countDHT[j]++; }
    }
    for (int j = 0; j < 6; j++) {
      if (!isnan(dsReadings[j][i])) { avgDS[j] += dsReadings[j][i]; countDS[j]++; }
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
    float totalAvg = 0;
    int validCount = 0;
    float allTemps[8];

    for (int j = 0; j < 2; j++) {
      allTemps[j] = avgDHT[j];
      if (!isnan(avgDHT[j])) { totalAvg += avgDHT[j]; validCount++; }
    }
    for (int j = 0; j < 6; j++) {
      allTemps[j + 2] = avgDS[j];
      if (!isnan(avgDS[j])) { totalAvg += avgDS[j]; validCount++; }
    }

    float avgTemp = validCount > 0 ? totalAvg / validCount : NAN;

    // Actuators
    digitalWrite(RELAY_FAN1, avgTemp >= 36 ? LOW : HIGH);
    digitalWrite(RELAY_FAN2, avgTemp >= 37.5 ? LOW : HIGH);
    digitalWrite(RELAY_PUMP, avgTemp >= 39 ? LOW : HIGH);

    String pumpState = digitalRead(RELAY_PUMP) == LOW ? "ON" : "OFF";
    String fan1State = digitalRead(RELAY_FAN1) == LOW ? "ON" : "OFF";
    String fan2State = digitalRead(RELAY_FAN2) == LOW ? "ON" : "OFF";

    float servoPosition = 180;
    String servoStatus = "Closed";
    if (avgTemp >= 36) { servoPosition = 0; servoStatus = "100% Open"; }
    else if (avgTemp >= 35) { servoPosition = 45; servoStatus = "75% Open"; }
    else if (avgTemp >= 34) { servoPosition = 90; servoStatus = "50% Open"; }
    else if (avgTemp >= 33) { servoPosition = 135; servoStatus = "25% Open"; }
    flapServo.write(servoPosition);

    // Prepare CSV data
    String data = String(tempAmbient) + ",";
    for (int i = 0; i < 8; i++) {
      data += String(allTemps[i]) + ",";
    }
    data += String(avgTemp) + ",";
    data += pumpState + "," + fan1State + "," + fan2State + "," + servoStatus;

    Serial.println(data);
    sendDataToWebApp(data);
  }

  delay(12000);
}

void sendDataToWebApp(String data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping data send.");
    return;
  }

  HTTPClient http;
  http.begin(webAppUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String requestBody = "data=" + data;
  int httpResponse = http.POST(requestBody);

  if (httpResponse == 200) {
    Serial.println("Data sent successfully!");
  } else {
    Serial.print("Failed to send data. Code: ");
    Serial.println(httpResponse);
  }

  http.end();
}
