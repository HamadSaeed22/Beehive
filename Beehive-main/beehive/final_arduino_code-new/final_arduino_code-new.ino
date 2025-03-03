#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

// Define DHT22 sensor pins
#define DHTPIN1 3
#define DHTPIN2 4
#define DHTPIN3 5

// Define DS18B20 data pin
#define ONE_WIRE_BUS 2

// Define relay pins
#define RELAY1 8
#define RELAY2 9
#define RELAY3 10
#define RELAY4 11

// Define servo motor pin
#define SERVO_PIN 6

// Define DHT sensor type
#define DHTTYPE DHT22

// Create DHT sensor objects
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);
DHT dht3(DHTPIN3, DHTTYPE);

// Set up OneWire and DallasTemperature for DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Create servo object
Servo flapServo;

void setup() {
  Serial.begin(115200);  // Use 115200 for better data transfer

  // Initialize DHT sensors
  dht1.begin();
  dht2.begin();
  dht3.begin();

  // Initialize DS18B20 sensors
  ds18b20.begin();

  // Initialize relay pins
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  // Ensure relays are on initially
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  // Initialize servo
  flapServo.attach(SERVO_PIN);
  flapServo.write(0); // Set initial position
}

void loop() {
  // Read temperatures from DHT22 sensors
  float tempDHT1 = dht1.readTemperature();
  float tempDHT2 = dht2.readTemperature();
  float tempDHT3 = dht3.readTemperature();

  // Read humidity from DHT22 sensors
  float humidityDHT1 = dht1.readHumidity();
  float humidityDHT2 = dht2.readHumidity();
  float humidityDHT3 = dht3.readHumidity();

  // Request temperatures from DS18B20 sensors
  ds18b20.requestTemperatures();
  float tempDS1 = ds18b20.getTempCByIndex(0);
  float tempDS2 = ds18b20.getTempCByIndex(1);

  if (isnan(tempDHT1) || isnan(tempDHT2) || isnan(tempDHT3) || tempDS1 == -127.00 || tempDS2 == -127.00) {
    Serial.println("Error: Sensor reading failed");
  } else {
    float avgTempDS = (tempDS1 + tempDS2) / 2.0;
    
    // Map temperature to servo position
    float servoPosition = map(constrain(avgTempDS, 32, 36), 32, 36, 0, 90);
    flapServo.write(servoPosition);

    Serial.print(tempDHT1); Serial.print(",");
    Serial.print(humidityDHT1); Serial.print(",");
    Serial.print(tempDHT2); Serial.print(",");
    Serial.print(humidityDHT2); Serial.print(",");
    Serial.print(tempDHT3); Serial.print(",");
    Serial.print(humidityDHT3); Serial.print(",");
    Serial.print(tempDS1); Serial.print(",");
    Serial.print(tempDS2); Serial.print(",");
    Serial.print(avgTempDS); Serial.print(",");
    Serial.println(servoPosition);
  }

  delay(2000); // Send data every 2 seconds
}
