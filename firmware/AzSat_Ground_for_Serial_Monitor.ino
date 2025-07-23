#include <LoRa.h>

// === LoRa Config ===
const long frequency = 433E6;
const int syncWord = 0x22;

uint8_t lastCommand[8];
bool commandPending = false;
String serialBuffer = "";

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!LoRa.begin(frequency)) {
    Serial.println("❌ LoRa init failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(syncWord);

  Serial.println("📡 Ground Station Ready");
  Serial.println("Type 8 comma-separated bytes to send command: packet,team,mode,led,buzzer,parachute,reset,custom");
}

void loop() {
  // === TELEMETRY RECEPTION ===
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("📥 Packet received. Size: ");
    Serial.println(packetSize);

    uint8_t buf[packetSize];
    LoRa.readBytes(buf, packetSize);

    if (packetSize == 24) {
      displayTelemetry(buf);
    } else {
      Serial.println("⚠️ Unknown packet size or format.");
    }

    // Resend pending command if not acknowledged
    if (commandPending) {
      Serial.println("🔁 Resending pending command (not yet acknowledged)");
      safeSendCommand(lastCommand);
    }
  }

  // === SERIAL INPUT BUFFERING (NON-BLOCKING) ===
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      processSerialCommand(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void processSerialCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  uint8_t cmd[8];
  int index = 0;

  while (input.length() > 0 && index < 8) {
    int commaIndex = input.indexOf(',');
    String token = (commaIndex >= 0) ? input.substring(0, commaIndex) : input;
    cmd[index++] = (uint8_t)token.toInt();
    if (commaIndex >= 0) input = input.substring(commaIndex + 1);
    else break;
  }

  if (index == 8) {
    memcpy(lastCommand, cmd, 8);
    commandPending = true;
    safeSendCommand(cmd);
  } else {
    Serial.println("❌ Error: Must send exactly 8 comma-separated values");
  }
}

void safeSendCommand(uint8_t *cmd) {
  // Ensure LoRa is not busy
  unsigned long start = millis();
  while (LoRa.beginPacket() == 0) {
    if (millis() - start > 200) {
      Serial.println("⚠️ LoRa busy, skipping this cycle");
      return;
    }
    delay(10);
  }

  Serial.print("📤 Sending command: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(cmd[i]);
    Serial.print(i < 7 ? "," : "\n");
    LoRa.write(cmd[i]);
  }

  if (!LoRa.endPacket()) {
    Serial.println("⚠️ Failed to complete transmission");
  }
  LoRa.idle();
}

void displayTelemetry(uint8_t *buf) {
  uint8_t i = 0;
  uint8_t packetID = buf[i++];
  uint8_t teamID = buf[i++];

  uint16_t uptime = (buf[i++] << 8) | buf[i++];

  uint8_t status = buf[i++];

  int16_t alt = (buf[i++] << 8) | buf[i++];
  int16_t vel = (buf[i++] << 8) | buf[i++];
  int16_t temp = (buf[i++] << 8) | buf[i++];
  uint8_t hum = buf[i++];
  uint16_t pres = (buf[i++] << 8) | buf[i++];

  int8_t ax = (int8_t)buf[i++];
  int8_t ay = (int8_t)buf[i++];
  int8_t az = (int8_t)buf[i++];
  int8_t gx = (int8_t)buf[i++];
  int8_t gy = (int8_t)buf[i++];
  int8_t gz = (int8_t)buf[i++];

  uint16_t light = (buf[i++] << 8) | buf[i++];
  uint8_t parachute = buf[i++];
  uint8_t bat = buf[i++];

  Serial.println("\n--- 🛰️ TELEMETRY RECEIVED ---");
  Serial.print("Packet ID: "); Serial.println(packetID);
  Serial.print("Team ID: "); Serial.println(teamID);
  Serial.print("Uptime: "); Serial.print(uptime); Serial.println(" s");
  Serial.print("Status Flags: "); Serial.println(status, BIN);

  Serial.print("Altitude: "); Serial.print(alt / 10.0); Serial.println(" m");
  Serial.print("Velocity: "); Serial.print(vel / 10.0); Serial.println(" m/s");
  Serial.print("Temperature: "); Serial.print(temp / 10.0); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Pressure: "); Serial.print(pres / 10.0); Serial.println(" hPa");

  Serial.print("Accel (X,Y,Z): ");
  Serial.print(ax / 10.0); Serial.print(", ");
  Serial.print(ay / 10.0); Serial.print(", ");
  Serial.print(az / 10.0); Serial.println(" m/s²");

  Serial.print("Gyro (X,Y,Z): ");
  Serial.print(gx / 10.0); Serial.print(", ");
  Serial.print(gy / 10.0); Serial.print(", ");
  Serial.print(gz / 10.0); Serial.println(" deg/s");

  Serial.print("Light: "); Serial.println(light);
  Serial.print("Parachute Detached: "); Serial.println(parachute);
  Serial.print("Battery: "); Serial.print(bat / 10.0); Serial.println(" V");

  Serial.println("-------------------------------");
}
