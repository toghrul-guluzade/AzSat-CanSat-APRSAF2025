#include <LoRa.h>

// === LoRa Config ===
const long frequency = 433E6;
const int syncWord = 0x22;

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
  LoRa.receive();

  Serial.println("✅ LoRa Ready (RX)");
  Serial.println("Type: <packet_id>,<team_id>,<buzzer (0–3)>,<parachute (0–3)>");
}

void loop() {
  // === SERIAL INPUT TO SEND COMMAND ===
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    int values[4], idx = 0;
    while (input.length() > 0 && idx < 4) {
      int sep = input.indexOf(',');
      String token = (sep == -1) ? input : input.substring(0, sep);
      input = (sep == -1) ? "" : input.substring(sep + 1);
      values[idx++] = token.toInt();
    }

    if (idx == 4) {
      uint8_t cmd[4] = {
        (uint8_t)values[0],
        (uint8_t)values[1],
        (uint8_t)(values[2] & 0x03),
        (uint8_t)(values[3] & 0x03)
      };

      Serial.print("📤 Sending command for 5 seconds: ");
      for (int i = 0; i < 4; i++) {
        Serial.print("<"); Serial.print(cmd[i]); Serial.print("> ");
      }
      Serial.println();

      unsigned long startTime = millis();
      while (millis() - startTime < 5000) {
        LoRa.beginPacket();
        LoRa.write(cmd, 4);
        LoRa.endPacket();

        delay(250);  // Repeat every 250ms (20x in 5 sec)
      }

      LoRa.receive();  // Go back to RX mode
      Serial.println("✅ Command send complete.");
    } else {
      Serial.println("⚠️ Invalid input. Use format: <id>,<team>,<buzzer>,<parachute>");
    }
  }

  // === TELEMETRY RECEPTION ===
  int packetSize = LoRa.parsePacket();
  if (packetSize == 24) {
    uint8_t buf[25];
    LoRa.readBytes(buf, 25);
    displayTelemetry(buf);
    LoRa.receive();
  } else if (packetSize > 0) {
    Serial.print("⚠️ Unknown packet size: ");
    Serial.println(packetSize);
    while (LoRa.available()) LoRa.read();
    LoRa.receive();
  }

  delay(50);
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

  Serial.print("<"); Serial.print(packetID); Serial.print("> ");
  Serial.print("<"); Serial.print(teamID); Serial.print("> ");
  Serial.print("<"); Serial.print(uptime); Serial.print("> ");
  Serial.print("<"); Serial.print(status); Serial.print("> ");
  Serial.print("<"); Serial.print(alt / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(vel / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(temp / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(hum); Serial.print("> ");
  Serial.print("<"); Serial.print(pres / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(ax / 10.0, 1); Serial.print(",");
  Serial.print(ay / 10.0, 1); Serial.print(",");
  Serial.print(az / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(gx / 10.0, 1); Serial.print(",");
  Serial.print(gy / 10.0, 1); Serial.print(",");
  Serial.print(gz / 10.0, 1); Serial.print("> ");
  Serial.print("<"); Serial.print(light); Serial.print("> ");
  Serial.print("<"); Serial.print(parachute); Serial.print("> ");
  Serial.print("<"); Serial.print(bat / 10.0, 1); Serial.println(">");
}
