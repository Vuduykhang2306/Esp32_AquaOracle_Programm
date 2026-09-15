/*
 * AquaSmart - đo chất lượng nước ao nuôi rồi gửi lên Supabase.
 *
 * Phần cứng: ESP32-WROOM-32E, board Beark, module relay 4 kênh
 * Cảm biến:  DS18B20 (nhiệt độ), pH, TDS, độ đục
 * Chu kỳ:    5 phút đo một lần
 *
 * Relay dùng để cắt nguồn cảm biến giữa các lần đo: chỉ bật đúng lúc đọc
 * rồi tắt ngay, không để cảm biến ăn điện liên tục.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Cấu hình WiFi
// Khai báo trong secrets.h (đã gitignore). Xem secrets.example.h.
#include "secrets.h"

// Cấu hình Supabase
// SUPABASE_URL va SUPABASE_ANON_KEY cung nam trong secrets.h.
const char* ESP_ID = "esp01";

// Chân cảm biến, theo sơ đồ board Beark
// ESP32 → Cảm biến (đọc dữ liệu)
#define PIN_TEMP 32              // GPIO32 → DS18B20 nhiệt độ (OneWire)
#define PIN_PH 34                // GPIO34 → cảm biến pH (analog)
#define PIN_TURBIDITY 33         // GPIO33 → cảm biến độ đục (analog)
#define PIN_TDS 35               // GPIO35 → cảm biến TDS (analog)

// ESP32 → Relay 4 kênh (điều khiển nguồn cảm biến qua Beark Board)
#define RELAY_1 17               // GPIO17 → Relay 1 (IN1)
#define RELAY_2 16               // GPIO16 → Relay 2 (IN2)
#define RELAY_3 0                // GPIO0  → Relay 3 (IN3)
#define RELAY_4 15               // GPIO15 → Relay 4 (IN4)

// Relay nào cấp nguồn cho cảm biến nào
// Từ sơ đồ, các relay kết nối VCC cảm biến qua Beark Board:
#define RELAY_TEMP       RELAY_1    // Relay 1 → VCC CB Nhiệt độ
#define RELAY_PH         RELAY_2    // Relay 2 → VCC CB pH
#define RELAY_TDS        RELAY_3    // Relay 3 → VCC CB TDS
#define RELAY_TURBIDITY  RELAY_4    // Relay 4 → VCC CB Turbidity

// Thiết lập DS18B20
OneWire oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);

// Biến toàn cục
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2 * 60 * 1000;  // 2 phút
const unsigned long SENSOR_WARMUP_TIME = 5000;      // 5 giây ổn định

// Struct chứa một lần đo
struct SensorData {
  float temperature;
  float ph;
  float tds;
  float turbidity;
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("AQUASMART - ESP32 BEARK BOARD v2.0");
  Serial.println("DS18B20 + Relay Power Management");
  
  Serial.println("\nCẤU HÌNH CHÂN:");
  Serial.println("CẢM BIẾN → ESP32");
  Serial.printf("CB Nhiệt độ → GPIO%d (DS18B20)\n", PIN_TEMP);
  Serial.printf("CB pH → GPIO%d (Analog)\n", PIN_PH);
  Serial.printf("CB TDS → GPIO%d (Analog)\n", PIN_TDS);
  Serial.printf("CB Turbidity → GPIO%d (Analog)\n", PIN_TURBIDITY);
  Serial.println("RELAY → ESP32");
  Serial.printf("Relay 1 (Nhiệt độ) → GPIO%d\n", RELAY_1);
  Serial.printf("Relay 2 (pH) → GPIO%d\n", RELAY_2);
  Serial.printf("Relay 3 (TDS) → GPIO%d\n", RELAY_3);
  Serial.printf("Relay 4 (Turbidity)→ GPIO%d\n", RELAY_4);
  
  // Khởi tạo GPIO
  initGPIO();
  
  // TẮT tất cả relay (TẮT tất cả cảm biến)
  disableAllSensors();
  
  // Khởi tạo DS18B20
  tempSensor.begin();
  Serial.println("DS18B20 khởi tạo thành công");
  
  // Kết nối WiFi
  connectWiFi();
  
  Serial.println("\nHệ thống sẵn sàng!");
  Serial.println("Kiểm tra: Tất cả đèn relay phải TẮT ở chế độ chờ");
}

void loop() {
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi ngắt! Đang kết nối lại...");
    connectWiFi();
  }
  
  // Chu kỳ đo 5 phút
  if (millis() - lastSendTime >= SEND_INTERVAL || lastSendTime == 0) {
    Serial.println("BẮT ĐẦU CHU KỲ ĐỌC CẢM BIẾN");
    
    // Đọc tất cả cảm biến (relay tự động BẬT/TẮT trong hàm)
    SensorData data = readAllSensors();
    
    // Hiển thị kết quả
    displaySensorData(data);
    
    // Gửi lên Supabase
    sendToSupabase(data);
    
    lastSendTime = millis();
    
    Serial.printf("⏰ Chu kỳ tiếp theo: 2 phút\n");
    Serial.println("Tất cả cảm biến đã TẮT (đèn relay TẮT)\n");
  }
  
  delay(1000);
}

// Khởi tạo GPIO
void initGPIO() {
  Serial.println("Đang khởi tạo GPIO...");
  
  // Cấu hình chân Analog Input (ADC)
  pinMode(PIN_PH, INPUT);
  pinMode(PIN_TDS, INPUT);
  pinMode(PIN_TURBIDITY, INPUT);
  // PIN_TEMP (GPIO32) tự động cấu hình bởi thư viện OneWire
  
  // Cấu hình Relay Output (mặc định HIGH = TẮT cho relay active LOW)
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT);
  
  // TẮT tất cả relay ngay từ đầu
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);
  digitalWrite(RELAY_3, HIGH);
  digitalWrite(RELAY_4, HIGH);
  
  Serial.println("GPIO khởi tạo - Tất cả relay TẮT");
}

// Tắt hết cảm biến
void disableAllSensors() {
  digitalWrite(RELAY_TEMP, HIGH);       // TẮT CB Nhiệt độ
  digitalWrite(RELAY_PH, HIGH);         // TẮT CB pH
  digitalWrite(RELAY_TDS, HIGH);        // TẮT CB TDS
  digitalWrite(RELAY_TURBIDITY, HIGH);  // TẮT CB Turbidity
  
  Serial.println("Đã TẮT tất cả cảm biến (đèn relay TẮT)");
}

// Kết nối WiFi
void connectWiFi() {
  Serial.print("Đang kết nối WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi kết nối thành công!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\nKết nối thất bại!");
    Serial.println("Khởi động lại sau 10s...");
    delay(10000);
    ESP.restart();
  }
}

// Đọc lần lượt từng cảm biến: bật relay, đọc, rồi tắt relay
SensorData readAllSensors() {
  SensorData data;
  
  // 1. ĐỌC NHIỆT ĐỘ DS18B20
  Serial.println("\n[1/4] Đang đọc NHIỆT ĐỘ...");
  digitalWrite(RELAY_TEMP, LOW);  // BẬT relay (đèn sáng)
  Serial.println("Relay 1 (GPIO17) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.temperature = readTemperature();
  digitalWrite(RELAY_TEMP, HIGH); // TẮT relay (đèn tắt)
  Serial.println("Relay 1 (GPIO17) TẮT");
  delay(200);
  
  // 2. ĐỌC pH
  Serial.println("\n[2/4] Đang đọc pH...");
  digitalWrite(RELAY_PH, LOW);    // BẬT relay (đèn sáng)
  Serial.println("Relay 2 (GPIO16) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.ph = readPH();
  digitalWrite(RELAY_PH, HIGH);   // TẮT relay (đèn tắt)
  Serial.println("Relay 2 (GPIO16) TẮT");
  delay(200);
  
  // 3. ĐỌC TDS
  Serial.println("\n[3/4] Đang đọc TDS...");
  digitalWrite(RELAY_TDS, LOW);   // BẬT relay (đèn sáng)
  Serial.println("Relay 3 (GPIO0) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.tds = readTDS();
  digitalWrite(RELAY_TDS, HIGH);  // TẮT relay (đèn tắt)
  Serial.println("Relay 3 (GPIO0) TẮT");
  delay(200);
  
  // 4. ĐỌC ĐỘ ĐỤC
  Serial.println("\n[4/4] Đang đọc ĐỘ ĐỤC...");
  digitalWrite(RELAY_TURBIDITY, LOW);  // BẬT relay (đèn sáng)
  Serial.println("Relay 4 (GPIO15) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.turbidity = readTurbidity();
  digitalWrite(RELAY_TURBIDITY, HIGH); // TẮT relay (đèn tắt)
  Serial.println("Relay 4 (GPIO15) TẮT");
  delay(200);
  
  return data;
}

// Đọc nhiệt độ DS18B20
float readTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  if (temp == DEVICE_DISCONNECTED_C || temp < -50 || temp > 100) {
    Serial.println("Lỗi cảm biến!");
    return -999;
  }
  
  Serial.printf("Nhiệt độ: %.2f °C\n", temp);
  return temp;
}

// Đọc pH
float readPH() {
  int adcValue = 0;
  for (int i = 0; i < 10; i++) {
    adcValue += analogRead(PIN_PH);
    delay(10);
  }
  adcValue /= 10;
  
  float voltage = adcValue * (3.3 / 4095.0);
  float ph = 7.0 + ((2.5 - voltage) / 0.18);
  ph = constrain(ph, 0.0, 14.0);
  
  Serial.printf("pH: %.2f (ADC: %d, V: %.3f)\n", ph, adcValue, voltage);
  return ph;
}

// Đọc TDS
float readTDS() {
  int adcValue = 0;
  for (int i = 0; i < 10; i++) {
    adcValue += analogRead(PIN_TDS);
    delay(10);
  }
  adcValue /= 10;
  
  float voltage = adcValue * (3.3 / 4095.0);
  float tds = (133.42 * pow(voltage, 3) - 255.86 * pow(voltage, 2) + 857.39 * voltage) * 0.5;
  tds = constrain(tds, 0, 2000);
  
  Serial.printf("TDS: %.1f ppm (ADC: %d, V: %.3f)\n", tds, adcValue, voltage);
  return tds;
}

// Đọc độ đục
float readTurbidity() {
  int adcValue = 0;
  for (int i = 0; i < 10; i++) {
    adcValue += analogRead(PIN_TURBIDITY);
    delay(10);
  }
  adcValue /= 10;
  
  float voltage = adcValue * (3.3 / 4095.0);
  float turbidity = -1120.4 * pow(voltage, 2) + 5742.3 * voltage - 4352.9;
  turbidity = constrain(turbidity, 0, 1000);
  
  Serial.printf("Độ đục: %.1f NTU (ADC: %d, V: %.3f)\n", turbidity, adcValue, voltage);
  return turbidity;
}

// In kết quả ra Serial
void displaySensorData(SensorData data) {
  Serial.println("TỔNG HỢP DỮ LIỆU CẢM BIẾN");
  Serial.printf("Nhiệt độ: %6.2f °C\n", data.temperature);
  Serial.printf("pH: %6.2f\n", data.ph);
  Serial.printf("TDS: %6.1f ppm\n", data.tds);
  Serial.printf("Độ đục: %6.1f NTU\n", data.turbidity);
  
  checkThresholds(data);
}

// So sánh với ngưỡng an toàn
void checkThresholds(SensorData data) {
  bool alert = false;
  
  Serial.println("KIỂM TRA NGƯỠNG:");
  
  if (data.temperature < 25 || data.temperature > 34) {
    Serial.println("Nhiệt độ ngoài ngưỡng!");
    alert = true;
  }
  
  if (data.ph < 6.8 || data.ph > 9.0) {
    Serial.println("pH ngoài ngưỡng!");
    alert = true;
  }
  
  if (data.tds > 30000) {
    Serial.println("TDS quá cao!");
    alert = true;
  }
  
  if (data.turbidity > 80) {
    Serial.println("Độ đục quá cao!");
    alert = true;
  }
  
  if (!alert) {
    Serial.println("Tất cả thông số OK");
  }
  Serial.println();
}

// Gửi lên Supabase
void sendToSupabase(SensorData data) {
  Serial.println("Đang gửi lên Supabase...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Không có WiFi!");
    return;
  }
  
  HTTPClient http;
  http.begin(SUPABASE_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Prefer", "return=minimal");
  
  StaticJsonDocument<512> doc;
  doc["esp_id"] = ESP_ID;
  doc["temperature"] = round(data.temperature * 100) / 100.0;
  doc["ph"] = round(data.ph * 100) / 100.0;
  doc["tds"] = round(data.tds * 10) / 10.0;
  doc["turbidity"] = round(data.turbidity * 10) / 10.0;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Payload:"+ jsonString);
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    Serial.printf("Gửi thành công! Code: %d\n", httpCode);
    String response = http.getString();
    if (response.length() > 0) {
      Serial.println("Response:"+ response);
    }
  } else {
    Serial.printf("Gửi thất bại! Error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

/*
 * Sơ đồ đấu dây theo board Beark
 *
 * Nguồn acquy 12V, qua hai mạch hạ áp 12V xuống 5V:
 *   - một cho ESP32 và board Beark
 *   - một cho VCC của module relay
 *
 * Luồng: ESP32 <-> board Beark <-> relay 4 kênh -> cảm biến
 *
 * Chân đọc dữ liệu:
 *   GPIO32   DS18B20 data, nối thẳng không qua relay
 *   GPIO34   pH, analog
 *   GPIO33   độ đục, analog
 *   GPIO35   TDS, analog
 *
 * Chân điều khiển relay:
 *   GPIO17   relay 1  ->  VCC cảm biến nhiệt độ
 *   GPIO16   relay 2  ->  VCC cảm biến pH
 *   GPIO0    relay 3  ->  VCC cảm biến TDS
 *   GPIO15   relay 4  ->  VCC cảm biến độ đục
 *
 * Module relay này tích cực mức thấp: LOW là BẬT, HIGH là TẮT. Lúc chờ thì cả
 * 4 đèn phải tắt, lúc đo chỉ sáng đúng một đèn. Mỗi chu kỳ 5 phút chỉ bật cảm
 * biến vài giây nên gần như toàn bộ thời gian cảm biến không có điện.
 */