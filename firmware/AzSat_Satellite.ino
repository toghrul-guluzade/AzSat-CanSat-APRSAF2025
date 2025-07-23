// --- Includes and Globals ---
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

const int motorPin1 = 7;
const int motorPin2 = 8;
const int lightSensorPin = A3;
const int voltagePin = A2;
const int buzzerPin = 3;

const long frequency = 433E6;
const int syncWord = 0x22;

Adafruit_MPU6050 mpu;
Adafruit_BME280 bme;

uint8_t packetID = 0;
uint16_t uptimeSeconds = 0;
unsigned long lastUptimeUpdate = 0;
unsigned long lastSend = 0;
const uint8_t teamID = 1;
float lastAltitude = 0;
float velocity = 0;
uint8_t statusFlags = 0b00000000;

// --- Setup ---
void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  if (!LoRa.begin(frequency)) {
    Serial.println("❌ LoRa init failed!");
    signalError();
    while (1);
  }

  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(syncWord);
  LoRa.receive(); // Start in RX mode

  if (!bme.begin(0x76)) {
    statusFlags |= 0b00000001;
    signalError(); while (1);
  }

  if (!mpu.begin()) {
    statusFlags |= 0b00010000;
    signalError(); while (1);
  }

  Serial.println("✅ Satellite Ready.");
}

// --- Main Loop ---
void loop() {
  unsigned long now = millis();

  // Update uptime
  if (now - lastUptimeUpdate >= 1000) {
    uptimeSeconds++;
    lastUptimeUpdate += 1000;
  }

  // Always check for command packets first
  int packetSize = LoRa.parsePacket();
  if (packetSize == 4) {
    handleCommand();
    LoRa.receive(); // resume RX
    return;
  }

  // Send telemetry every 1000ms
  if (now - lastSend >= 1000) {
    lastSend = now;
    sendTelemetry();
    LoRa.receive(); // go back to RX mode
  }
}

// --- Command Handler ---
void handleCommand() {
  uint8_t cmd[4];
  int received = LoRa.readBytes(cmd, 4);
  if (received != 4) return; // prevent corruption

  uint8_t packet_id = cmd[0];
  uint8_t team_id = cmd[1];
  uint8_t buzzer = cmd[2] & 0x03;
  uint8_t parachute = cmd[3] & 0x03;

  Serial.print("📥 Command: <"); Serial.print(packet_id);
  Serial.print("> <"); Serial.print(team_id);
  Serial.print("> <Buzzer:"); Serial.print(buzzer);
  Serial.print("> <Parachute:"); Serial.print(parachute); Serial.println(">");

  // Buzzer
  if (buzzer == 1) beep(100);
  else if (buzzer == 2) { beep(100); delay(100); beep(100); }
  else if (buzzer == 3) beep(500);

  // Motor
  if (parachute == 1) {
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);
    delay(5000);
  } else if (parachute == 2) {
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, HIGH);
    delay(5000);
  }

  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
}

void beep(int d) {
  digitalWrite(buzzerPin, HIGH); delay(d);
  digitalWrite(buzzerPin, LOW);
}

// --- Send Telemetry ---
void sendTelemetry() {
  statusFlags &= 0b00010001;

  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0F;
  float a = bme.readAltitude(1013.25);

  if (!isfinite(t)) t = 0;
  if (!isfinite(h)) h = 0;
  if (!isfinite(p)) p = 1013.25;
  if (!isfinite(a)) a = 0;

  if (t < -40 || t > 85) statusFlags |= 0b00000010;
  if (h < 0 || h > 100) statusFlags |= 0b00000100;
  if (p < 300 || p > 1100) statusFlags |= 0b00001000;

  velocity = a - lastAltitude;
  lastAltitude = a;
  if (!isfinite(velocity)) velocity = 0;
  if (velocity > -5 || velocity < -10) statusFlags |= 0b10000000;

  sensors_event_t acc, gyro, temp_mpu;
  mpu.getEvent(&acc, &gyro, &temp_mpu);

  int light = analogRead(lightSensorPin);
  float vRaw = analogRead(voltagePin) / 1023.0 * 5.0;
  float voltage = vRaw * 5.0 * 1.088;
  if (!isfinite(voltage)) voltage = 0;
  if (voltage < 3.3) statusFlags |= 0b01000000;
  uint8_t bat = voltage * 10;

  uint8_t pkt[25];
  uint8_t i = 0;

  pkt[i++] = packetID++;
  pkt[i++] = teamID;
  pkt[i++] = highByte(uptimeSeconds);
  pkt[i++] = lowByte(uptimeSeconds);
  pkt[i++] = statusFlags;

  int16_t alt_i = a * 10;
  pkt[i++] = highByte(alt_i); pkt[i++] = lowByte(alt_i);

  int16_t vel_i = velocity * 10;
  pkt[i++] = highByte(vel_i); pkt[i++] = lowByte(vel_i);

  int16_t temp_i = t * 10;
  pkt[i++] = highByte(temp_i); pkt[i++] = lowByte(temp_i);

  pkt[i++] = h;
  uint16_t pres_i = p * 10;
  pkt[i++] = highByte(pres_i); pkt[i++] = lowByte(pres_i);

  pkt[i++] = constrain(acc.acceleration.x * 10, -128, 127);
  pkt[i++] = constrain(acc.acceleration.y * 10, -128, 127);
  pkt[i++] = constrain(acc.acceleration.z * 10, -128, 127);

  pkt[i++] = constrain(gyro.gyro.x * 10, -128, 127);
  pkt[i++] = constrain(gyro.gyro.y * 10, -128, 127);
  pkt[i++] = constrain(gyro.gyro.z * 10, -128, 127);

  pkt[i++] = highByte(light); pkt[i++] = lowByte(light);
  pkt[i++] = 0;
  pkt[i++] = bat;

  LoRa.beginPacket();
  LoRa.write(pkt, i);
  LoRa.endPacket();

  Serial.print("📤 Sent ID: "); Serial.print(packetID - 1);
  Serial.print(" | Alt: "); Serial.print(a, 1);
  Serial.print(" | Vel: "); Serial.println(velocity, 2);
}

void signalError() {
  digitalWrite(buzzerPin, HIGH); delay(1000);
  digitalWrite(buzzerPin, LOW);
}
