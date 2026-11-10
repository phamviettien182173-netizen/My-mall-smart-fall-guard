#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

const char* ssid = "TEN_WIFI_NHA_BAN";
const char* password = "MAT_KHAU_WIFI";
const char* mqtt_server = "192.168.1.X"; // Địa chỉ IP máy tính của bạn

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_MPU6050 mpu;

const char* DEVICE_ID = "esp8266_001";
const int SOS_BUTTON_PIN = D5;

void setup() {
  Serial.begin(115200);
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);
  
  // Khởi tạo MPU6050
  Wire.begin(D2, D1); // SDA = D2, SCL = D1
  if (!mpu.begin()) {
    Serial.println("Lỗi: Không tìm thấy MPU6050!");
    while (1) delay(10);
  }

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi đã kết nối!");

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect(DEVICE_ID)) {
      // Gửi trạng thái ONLINE khi vừa kết nối thành công
      client.publish("fallguard/v1/devices/esp8266_001/status", "ONLINE");
    } else {
      delay(2000);
    }
  }
}

unsigned long lastTelemetry = 0;

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Tính Vector gia tốc tổng hợp (SVM) tính theo đơn vị g
  float ax = a.acceleration.x / 9.81;
  float ay = a.acceleration.y / 9.81;
  float az = a.acceleration.z / 9.81;
  float svm = sqrt(ax*ax + ay*ay + az*az);

  // 1. Thuật toán phát hiện té ngã (Free fall + Va chạm)
  if (svm > 2.5) { // Phát hiện cú va đập mạnh sau khi rơi
    String alertPayload = "{\"device_id\":\"" + String(DEVICE_ID) + "\",\"event\":\"FALL_CONFIRMED\",\"svm_peak\":" + String(svm) + "}";
    client.publish("fallguard/v1/devices/esp8266_001/alert", alertPayload.c_str());
    Serial.println("🚨 CANH BAO TE NGA!");
    delay(3000); // Tránh trùng lặp cảnh báo liền kề
  }

  // 2. Nút bấm SOS cứng trên mạch
  if (digitalRead(SOS_BUTTON_PIN) == LOW) {
    String sosPayload = "{\"device_id\":\"" + String(DEVICE_ID) + "\",\"event\":\"SOS_PRESSED\",\"svm_peak\":" + String(svm) + "}";
    client.publish("fallguard/v1/devices/esp8266_001/alert", sosPayload.c_str());
    Serial.println("🆘 SOS KHAN CAP!");
    delay(2000);
  }

  // 3. Gửi telemetry định kỳ 2 giây/lần
  if (millis() - lastTelemetry > 2000) {
    lastTelemetry = millis();
    String telePayload = "{\"device_id\":\"" + String(DEVICE_ID) + "\",\"svm\":" + String(svm) + "}";
    client.publish("fallguard/v1/devices/esp8266_001/telemetry", telePayload.c_str());
  }
}