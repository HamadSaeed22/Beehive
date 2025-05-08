#include <DHT.h>  // Library for DHT sensors
#include <OneWire.h>  // Library for 1-Wire sensors
#include <DallasTemperature.h>  // Library for DS18B20 temp sensors
#include <Servo.h>  // Library for servo motor

// Define pins for sensors and actuators
#define AMBIENT_DHTPIN 8
#define DHTPIN2 5
#define DHTPIN3 6
#define ONE_WIRE_BUS 9
#define RELAY_PUMP 11
#define RELAY_FAN1 12
#define RELAY_FAN2 13
#define SERVO_PIN 7
#define DHTTYPE DHT22

DHT ambientDHT(AMBIENT_DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);
Servo flapServo;

const int numReadings = 5;  // Number of samples to average
float dhtReadings[2][numReadings] = {0};
float dsReadings[6][numReadings] = {0};
float ambientReadings[numReadings] = {0};
float humReadings[3][numReadings] = {0};

int dhtIndex = 0, dsIndex = 0, ambientIndex = 0;
unsigned long lastReadTime = 0;
const unsigned long interval = 12000;  // Time between readings (12 seconds)

// Pump timing control
bool pumpIsRunning = false;
bool pumpCooldown = false;
unsigned long pumpStartMillis = 0;
unsigned long pumpCooldownMillis = 0;
const unsigned long pumpRunDuration = 12000;      // 12s ON time
const unsigned long pumpCooldownDuration = 60000; // 60s cooldown

void setup() {
    Serial.begin(115200);
    ambientDHT.begin(); dht2.begin(); dht3.begin(); ds18b20.begin();
    pinMode(RELAY_PUMP, OUTPUT); pinMode(RELAY_FAN1, OUTPUT); pinMode(RELAY_FAN2, OUTPUT);
    digitalWrite(RELAY_PUMP, HIGH); digitalWrite(RELAY_FAN1, HIGH); digitalWrite(RELAY_FAN2, HIGH);
    flapServo.attach(SERVO_PIN); flapServo.write(180);
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastReadTime >= interval) {
        lastReadTime = currentMillis;

        // Read temperature and humidity sensors
        float newAmbient = ambientDHT.readTemperature();
        float newDHT2 = dht2.readTemperature();
        float newDHT3 = dht3.readTemperature();
        float humAmbient = ambientDHT.readHumidity();
        float humDHT2 = dht2.readHumidity();
        float humDHT3 = dht3.readHumidity();

        // Read DS18B20 sensors
        float newDS[6];
        ds18b20.requestTemperatures();
        for (int i = 0; i < 6; i++) newDS[i] = ds18b20.getTempCByIndex(i);

        // Store new readings or keep old if invalid
        ambientReadings[ambientIndex] = !isnan(newAmbient) ? newAmbient : ambientReadings[ambientIndex];
        dhtReadings[0][dhtIndex] = !isnan(newDHT2) ? newDHT2 : dhtReadings[0][dhtIndex];
        dhtReadings[1][dhtIndex] = !isnan(newDHT3) ? newDHT3 : dhtReadings[1][dhtIndex];
        for (int i = 0; i < 6; i++) dsReadings[i][dsIndex] = !isnan(newDS[i]) ? newDS[i] : dsReadings[i][dsIndex];
        humReadings[0][ambientIndex] = !isnan(humAmbient) ? humAmbient : humReadings[0][ambientIndex];
        humReadings[1][dhtIndex] = !isnan(humDHT2) ? humDHT2 : humReadings[1][dhtIndex];
        humReadings[2][dhtIndex] = !isnan(humDHT3) ? humDHT3 : humReadings[2][dhtIndex];

        // Rotate buffer indices
        dhtIndex = (dhtIndex + 1) % numReadings;
        dsIndex = (dsIndex + 1) % numReadings;
        ambientIndex = (ambientIndex + 1) % numReadings;

        // Initialize averages and counters
        float avgDHT[2] = {0}, avgDS[6] = {0}, avgAmbient = 0, avgHumidity[3] = {0};
        int countDHT[2] = {0}, countDS[6] = {0}, countAmbient = 0, countHumidity[3] = {0};

        // Sum up readings
        for (int i = 0; i < numReadings; i++) {
            if (!isnan(ambientReadings[i])) { avgAmbient += ambientReadings[i]; countAmbient++; }
            for (int j = 0; j < 2; j++) if (!isnan(dhtReadings[j][i])) { avgDHT[j] += dhtReadings[j][i]; countDHT[j]++; }
            for (int j = 0; j < 6; j++) if (!isnan(dsReadings[j][i])) { avgDS[j] += dsReadings[j][i]; countDS[j]++; }
            for (int j = 0; j < 3; j++) if (!isnan(humReadings[j][i])) { avgHumidity[j] += humReadings[j][i]; countHumidity[j]++; }
        }

        // Finalize averages
        avgAmbient = countAmbient > 0 ? avgAmbient / countAmbient : NAN;
        for (int j = 0; j < 2; j++) avgDHT[j] = countDHT[j] > 0 ? avgDHT[j] / countDHT[j] : NAN;
        for (int j = 0; j < 6; j++) avgDS[j] = countDS[j] > 0 ? avgDS[j] / countDS[j] : NAN;
        for (int j = 0; j < 3; j++) avgHumidity[j] = countHumidity[j] > 0 ? avgHumidity[j] / countHumidity[j] : NAN;

        // When buffer full, process control logic
        if (dhtIndex == 0) {
            // Calculate average temp from DHT only
            float avgDHTTemp = (!isnan(avgDHT[0]) && !isnan(avgDHT[1])) ? (avgDHT[0] + avgDHT[1]) / 2.0 : NAN;
            // Calculate overall average temp from DHT and DS sensors
            float totalAvg = 0; int validCount = 0;
            for (int j = 0; j < 2; j++) if (!isnan(avgDHT[j])) { totalAvg += avgDHT[j]; validCount++; }
            for (int j = 0; j < 6; j++) if (!isnan(avgDS[j])) { totalAvg += avgDS[j]; validCount++; }
            float avgTemp = validCount > 0 ? totalAvg / validCount : NAN;

            float humidityAvg = (!isnan(avgHumidity[1]) && !isnan(avgHumidity[2])) ? (avgHumidity[1] + avgHumidity[2]) / 2.0 : NAN;

            // Fan control
            digitalWrite(RELAY_FAN1, avgDHTTemp >= 42 ? LOW : HIGH);
            digitalWrite(RELAY_FAN2, avgDHTTemp >= 43 ? LOW : HIGH);

            // Pump control with 12s timer and cooldown
            if (avgDHTTemp >= 44 && !pumpIsRunning && !pumpCooldown) {
                digitalWrite(RELAY_PUMP, LOW);
                pumpIsRunning = true;
                pumpStartMillis = millis();
            }
            if (pumpIsRunning && (millis() - pumpStartMillis >= pumpRunDuration)) {
                digitalWrite(RELAY_PUMP, HIGH);
                pumpIsRunning = false;
                pumpCooldown = true;
                pumpCooldownMillis = millis();
            }
            if (pumpCooldown && (millis() - pumpCooldownMillis >= pumpCooldownDuration)) {
                pumpCooldown = false;
            }

            // Servo control based on avgDHTTemp
            float servoPosition = 180;
            String servoStatus = "Closed";
            if (avgDHTTemp >= 41) { servoPosition = 0; servoStatus = "100% Open"; }
            else if (avgDHTTemp >= 40) { servoPosition = 45; servoStatus = "75% Open"; }
            else if (avgDHTTemp >= 39) { servoPosition = 90; servoStatus = "50% Open"; }
            else if (avgDHTTemp >= 38) { servoPosition = 135; servoStatus = "25% Open"; }
            flapServo.write(servoPosition);

            // Output data to Serial in CSV format
            String pumpState = digitalRead(RELAY_PUMP) == LOW ? "ON" : "OFF";
            String fan1State = digitalRead(RELAY_FAN1) == LOW ? "ON" : "OFF";
            String fan2State = digitalRead(RELAY_FAN2) == LOW ? "ON" : "OFF";
            Serial.print(avgAmbient); Serial.print(",");
            Serial.print(avgHumidity[0]); Serial.print(",");
            Serial.print(avgDHT[0]); Serial.print(",");
            Serial.print(avgHumidity[1]); Serial.print(",");
            Serial.print(avgDHT[1]); Serial.print(",");
            Serial.print(avgHumidity[2]); Serial.print(",");
            for (int i = 0; i < 6; i++) { Serial.print(avgDS[i]); Serial.print(","); }
            Serial.print(avgDHTTemp); Serial.print(",");  // Print DHT-only average
            Serial.print(avgTemp); Serial.print(",");     // Print full average including DS
            Serial.print(humidityAvg); Serial.print(",");
            Serial.print(pumpState); Serial.print(",");
            Serial.print(fan1State); Serial.print(",");
            Serial.print(fan2State); Serial.print(",");
            Serial.println(servoStatus);
        }
    }
}