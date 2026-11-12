#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// Cấu hình mạng WiFi và MQTT Broker
const char* ssid = "LHPND 2.4Ghz";
const char* password = "23456789";
const char* mqtt_server = "192.168.1.15";

// Cấu hình thiết bị
const char* DEVICE_ID = "esp8266_001";
const int SOS_BUTTON_PIN = D5;

WiFiClient espClient;
PubSubClient client(espClient);

bool mpuFound = false;
uint8_t mpuAddress = 0x68;
unsigned long lastTelemetry = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- HE THONG SMART FALL GUARD KHOI DONG ---");

  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);

  // Khởi tạo bus I2C trên cặp chân dự phòng D6 (SDA) và D7 (SCL)
  Wire.begin(D6, D7);
  Wire.setClock(100000);
  Wire.setClockStretchLimit(15000);

  // Đánh thức MPU6050 trực tiếp tại địa chỉ 0x68 (hoặc 0x69)
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // Thanh ghi PWR_MGMT_1
  Wire.write(0x00); // Đánh thức cảm biến
  if (Wire.endTransmission() == 0) {
    mpuAddress = 0x68;
    mpuFound = true;
    Serial.println("-> Da tim thay MPU6050 (0x68) tren chan D6/D7!");
  } else {
    Wire.beginTransmission(0x69);
    Wire.write(0x6B);
    Wire.write(0x00);
    if (Wire.endTransmission() == 0) {
      mpuAddress = 0x69;
      mpuFound = true;
      Serial.println("-> Da tim thay MPU6050 (0x69) tren chan D6/D7!");
    } else {
      mpuFound = false;
      Serial.println("⚠️ Khong tim thay MPU6050, chay che do an toan.");
    }
  }

  // Kết nối WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n-> WiFi da ket noi thanh cong!");

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT Broker...");
    if (client.connect(DEVICE_ID)) {
      Serial.println(" thanh cong!");
      client.publish("fallguard/v1/devices/esp8266_001/status", "ONLINE");
    } else {
      Serial.print(" loi, rc=");
      Serial.print(client.state());
      Serial.println(" thu lai sau 2s...");
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float svm = 1.0;

  if (mpuFound) {
    // Đọc 6 byte gia tốc (X, Y, Z) từ thanh ghi 0x3B
    Wire.beginTransmission(mpuAddress);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(mpuAddress, (uint8_t)6, (uint8_t)true);

    if (Wire.available() == 6) {
      int16_t ax_raw = (Wire.read() << 8) | Wire.read();
      int16_t ay_raw = (Wire.read() << 8) | Wire.read();
      int16_t az_raw = (Wire.read() << 8) | Wire.read();

      float ax = ax_raw / 16384.0;
      float ay = ay_raw / 16384.0;
      float az = az_raw / 16384.0;
      svm = sqrt(ax * ax + ay * ay + az * az);
    }

    // Phát hiện té ngã khi tổng gia tốc vượt ngưỡng 2.5G
    if (svm > 2.5) {
      char alertBuf[128];
      snprintf(alertBuf, sizeof(alertBuf),
               "{\"device_id\":\"%s\",\"event\":\"FALL_CONFIRMED\",\"svm_peak\":%.2f}",
               DEVICE_ID, svm);
      client.publish("fallguard/v1/devices/esp8266_001/alert", alertBuf);
      Serial.println("🚨 CANH BAO: TE NGA!");
      delay(3000);
    }
  }

  // Nút nhấn khẩn cấp SOS (chân D5 nối GND)
  if (digitalRead(SOS_BUTTON_PIN) == LOW) {
    char sosBuf[128];
    snprintf(sosBuf, sizeof(sosBuf),
             "{\"device_id\":\"%s\",\"event\":\"SOS_PRESSED\",\"svm_peak\":%.2f}",
             DEVICE_ID, svm);
    client.publish("fallguard/v1/devices/esp8266_001/alert", sosBuf);
    Serial.println("🆘 SOS KHAN CAP!");
    delay(2000);
  }

  // Gửi dữ liệu Telemetry mỗi 2 giây
  if (millis() - lastTelemetry > 2000) {
    lastTelemetry = millis();
    char teleBuf[128];
    snprintf(teleBuf, sizeof(teleBuf),
             "{\"device_id\":\"%s\",\"svm\":%.2f}",
             DEVICE_ID, svm);
    client.publish("fallguard/v1/devices/esp8266_001/telemetry", teleBuf);

    Serial.print("Telemetry SVM: ");
    Serial.println(svm);
  }
}