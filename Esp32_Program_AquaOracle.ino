/*
 * ============================================================================
 * AQUASMART - HỆ THỐNG GIÁM SÁT CHẤT LƯỢNG NƯỚC AO NUÔI
 * ============================================================================
 * Phần cứng: ESP32-WROOM-32E + Beark Board
 * Cảm biến: TDS, Turbidity (Đục), Temperature (DS18B20 Waterproof), pH
 * Database: Supabase Realtime
 * Tần suất gửi: 5 phút/lần
 * Relay: Điều khiển nguồn cảm biến (BẬT khi đo, TẮT khi chờ)
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================================
// CẤU HÌNH WIFI
// ============================================================================
const char* WIFI_SSID = "CLBTINHOCHONGBANG";
const char* WIFI_PASSWORD = "th@ykhongcho";

// ============================================================================
// CẤU HÌNH SUPABASE
// ============================================================================
const char* SUPABASE_URL = "https://zkfchfopuqpngcyzdknd.supabase.co/rest/v1/water_quality";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InprZmNoZm9wdXFwbmdjeXpka25kIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTMyNTcxODUsImV4cCI6MjA2ODgzMzE4NX0.HnT1gKtBSyxBTzz5JwcxuA5SK_LGDDj-K8fPt_jXlR0";
const char* ESP_ID = "esp01";

// ============================================================================
// CẤU HÌNH CHÂN CẢM BIẾN (THEO SƠ ĐỒ BEARK BOARD)
// ============================================================================
// ESP32 → Cảm biến (đọc dữ liệu)
#define PIN_TEMP 32              // GPIO32 → CB Nhiệt độ DS18B20 (OneWire)
#define PIN_PH 34                // GPIO33 → CB pH (Analog)
#define PIN_TURBIDITY 33         // GPIO34 → CB Độ đục/Turbidity (Analog)
#define PIN_TDS 35               // GPIO35 → CB TDS (Analog)

// ESP32 → Relay 4 kênh (điều khiển nguồn cảm biến qua Beark Board)
#define RELAY_1 17               // GPIO17 → Relay 1 (IN1)
#define RELAY_2 16               // GPIO16 → Relay 2 (IN2)
#define RELAY_3 0                // GPIO0  → Relay 3 (IN3)
#define RELAY_4 15               // GPIO15 → Relay 4 (IN4)

// ============================================================================
// ÁNH XẠ RELAY → CẢM BIẾN (THEO SƠ ĐỒ CỦA BẠN)
// ============================================================================
// Từ sơ đồ, các relay kết nối VCC cảm biến qua Beark Board:
#define RELAY_TEMP       RELAY_1    // Relay 1 → VCC CB Nhiệt độ
#define RELAY_PH         RELAY_2    // Relay 2 → VCC CB pH
#define RELAY_TDS        RELAY_3    // Relay 3 → VCC CB TDS
#define RELAY_TURBIDITY  RELAY_4    // Relay 4 → VCC CB Turbidity

// ============================================================================
// THIẾT LẬP CẢM BIẾN DS18B20
// ============================================================================
OneWire oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);

// ============================================================================
// BIẾN TOÀN CỤC
// ============================================================================
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 2 * 60 * 1000;  // 2 phút
const unsigned long SENSOR_WARMUP_TIME = 5000;      // 5 giây ổn định

// ============================================================================
// CẤU TRÚC DỮ LIỆU
// ============================================================================
struct SensorData {
  float temperature;
  float ph;
  float tds;
  float turbidity;
};

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║   AQUASMART - ESP32 BEARK BOARD v2.0      ║");
  Serial.println("║   DS18B20 + Relay Power Management         ║");
  Serial.println("╚════════════════════════════════════════════╝");
  
  Serial.println("\n📋 CẤU HÌNH CHÂN:");
  Serial.println("┌─────────────────────────────────────────┐");
  Serial.println("│ CẢNH BIẾN → ESP32                       │");
  Serial.println("├─────────────────────────────────────────┤");
  Serial.printf("│ CB Nhiệt độ  → GPIO%d (DS18B20)        │\n", PIN_TEMP);
  Serial.printf("│ CB pH        → GPIO%d (Analog)         │\n", PIN_PH);
  Serial.printf("│ CB TDS       → GPIO%d (Analog)         │\n", PIN_TDS);
  Serial.printf("│ CB Turbidity → GPIO%d (Analog)         │\n", PIN_TURBIDITY);
  Serial.println("├─────────────────────────────────────────┤");
  Serial.println("│ RELAY → ESP32                           │");
  Serial.println("├─────────────────────────────────────────┤");
  Serial.printf("│ Relay 1 (Nhiệt độ) → GPIO%d            │\n", RELAY_1);
  Serial.printf("│ Relay 2 (pH)       → GPIO%d            │\n", RELAY_2);
  Serial.printf("│ Relay 3 (TDS)      → GPIO%d             │\n", RELAY_3);
  Serial.printf("│ Relay 4 (Turbidity)→ GPIO%d            │\n", RELAY_4);
  Serial.println("└─────────────────────────────────────────┘\n");
  
  // Khởi tạo GPIO
  initGPIO();
  
  // TẮT tất cả relay (TẮT tất cả cảm biến)
  disableAllSensors();
  
  // Khởi tạo DS18B20
  tempSensor.begin();
  Serial.println("✓ DS18B20 khởi tạo thành công");
  
  // Kết nối WiFi
  connectWiFi();
  
  Serial.println("\n✓ Hệ thống sẵn sàng!");
  Serial.println("💡 Kiểm tra: Tất cả đèn relay phải TẮT ở chế độ chờ");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi ngắt! Đang kết nối lại...");
    connectWiFi();
  }
  
  // Chu kỳ đo 5 phút
  if (millis() - lastSendTime >= SEND_INTERVAL || lastSendTime == 0) {
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("📊 BẮT ĐẦU CHU KỲ ĐỌC CẢM BIẾN");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    // Đọc tất cả cảm biến (relay tự động BẬT/TẮT trong hàm)
    SensorData data = readAllSensors();
    
    // Hiển thị kết quả
    displaySensorData(data);
    
    // Gửi lên Supabase
    sendToSupabase(data);
    
    lastSendTime = millis();
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.printf("⏰ Chu kỳ tiếp theo: 2 phút\n");
    Serial.println("💤 Tất cả cảm biến đã TẮT (đèn relay TẮT)\n");
  }
  
  delay(1000);
}

// ============================================================================
// KHỞI TẠO GPIO
// ============================================================================
void initGPIO() {
  Serial.println("⚙ Đang khởi tạo GPIO...");
  
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
  
  Serial.println("✓ GPIO khởi tạo - Tất cả relay TẮT");
}

// ============================================================================
// TẮT TẤT CẢ CẢM BIẾN
// ============================================================================
void disableAllSensors() {
  digitalWrite(RELAY_TEMP, HIGH);       // TẮT CB Nhiệt độ
  digitalWrite(RELAY_PH, HIGH);         // TẮT CB pH
  digitalWrite(RELAY_TDS, HIGH);        // TẮT CB TDS
  digitalWrite(RELAY_TURBIDITY, HIGH);  // TẮT CB Turbidity
  
  Serial.println("🔌 Đã TẮT tất cả cảm biến (đèn relay TẮT)");
}

// ============================================================================
// KẾT NỐI WIFI
// ============================================================================
void connectWiFi() {
  Serial.print("📡 Đang kết nối WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi kết nối thành công!");
    Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n✗ Kết nối thất bại!");
    Serial.println("   Khởi động lại sau 10s...");
    delay(10000);
    ESP.restart();
  }
}

// ============================================================================
// ĐỌC TẤT CẢ CẢM BIẾN (BẬT TỪNG CÁI, ĐỌC, TẮT)
// ============================================================================
SensorData readAllSensors() {
  SensorData data;
  
  // 1. ĐỌC NHIỆT ĐỘ DS18B20
  Serial.println("\n🌡 [1/4] Đang đọc NHIỆT ĐỘ...");
  digitalWrite(RELAY_TEMP, LOW);  // BẬT relay (đèn sáng)
  Serial.println("   💡 Relay 1 (GPIO17) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.temperature = readTemperature();
  digitalWrite(RELAY_TEMP, HIGH); // TẮT relay (đèn tắt)
  Serial.println("   💡 Relay 1 (GPIO17) TẮT");
  delay(200);
  
  // 2. ĐỌC pH
  Serial.println("\n⚗ [2/4] Đang đọc pH...");
  digitalWrite(RELAY_PH, LOW);    // BẬT relay (đèn sáng)
  Serial.println("   💡 Relay 2 (GPIO16) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.ph = readPH();
  digitalWrite(RELAY_PH, HIGH);   // TẮT relay (đèn tắt)
  Serial.println("   💡 Relay 2 (GPIO16) TẮT");
  delay(200);
  
  // 3. ĐỌC TDS
  Serial.println("\n💧 [3/4] Đang đọc TDS...");
  digitalWrite(RELAY_TDS, LOW);   // BẬT relay (đèn sáng)
  Serial.println("   💡 Relay 3 (GPIO0) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.tds = readTDS();
  digitalWrite(RELAY_TDS, HIGH);  // TẮT relay (đèn tắt)
  Serial.println("   💡 Relay 3 (GPIO0) TẮT");
  delay(200);
  
  // 4. ĐỌC ĐỘ ĐỤC
  Serial.println("\n🌊 [4/4] Đang đọc ĐỘ ĐỤC...");
  digitalWrite(RELAY_TURBIDITY, LOW);  // BẬT relay (đèn sáng)
  Serial.println("   💡 Relay 4 (GPIO15) BẬT");
  delay(SENSOR_WARMUP_TIME);
  data.turbidity = readTurbidity();
  digitalWrite(RELAY_TURBIDITY, HIGH); // TẮT relay (đèn tắt)
  Serial.println("   💡 Relay 4 (GPIO15) TẮT");
  delay(200);
  
  return data;
}

// ============================================================================
// ĐỌC DS18B20 (GPIO32)
// ============================================================================
float readTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  if (temp == DEVICE_DISCONNECTED_C || temp < -50 || temp > 100) {
    Serial.println("   ✗ Lỗi cảm biến!");
    return -999;
  }
  
  Serial.printf("   ✓ Nhiệt độ: %.2f °C\n", temp);
  return temp;
}

// ============================================================================
// ĐỌC pH (GPIO33)
// ============================================================================
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
  
  Serial.printf("   ✓ pH: %.2f (ADC: %d, V: %.3f)\n", ph, adcValue, voltage);
  return ph;
}

// ============================================================================
// ĐỌC TDS (GPIO35)
// ============================================================================
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
  
  Serial.printf("   ✓ TDS: %.1f ppm (ADC: %d, V: %.3f)\n", tds, adcValue, voltage);
  return tds;
}

// ============================================================================
// ĐỌC ĐỘ ĐỤC (GPIO34)
// ============================================================================
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
  
  Serial.printf("   ✓ Độ đục: %.1f NTU (ADC: %d, V: %.3f)\n", turbidity, adcValue, voltage);
  return turbidity;
}

// ============================================================================
// HIỂN THỊ DỮ LIỆU
// ============================================================================
void displaySensorData(SensorData data) {
  Serial.println("\n┌─────────────────────────────────────────┐");
  Serial.println("│       TỔNG HỢP DỮ LIỆU CẢM BIẾN        │");
  Serial.println("├─────────────────────────────────────────┤");
  Serial.printf("│ Nhiệt độ:   %6.2f °C                 │\n", data.temperature);
  Serial.printf("│ pH:         %6.2f                     │\n", data.ph);
  Serial.printf("│ TDS:        %6.1f ppm                 │\n", data.tds);
  Serial.printf("│ Độ đục:     %6.1f NTU                 │\n", data.turbidity);
  Serial.println("└─────────────────────────────────────────┘\n");
  
  checkThresholds(data);
}

// ============================================================================
// KIỂM TRA NGƯỠNG
// ============================================================================
void checkThresholds(SensorData data) {
  bool alert = false;
  
  Serial.println("🔍 KIỂM TRA NGƯỠNG:");
  
  if (data.temperature < 25 || data.temperature > 34) {
    Serial.println("   ⚠ Nhiệt độ ngoài ngưỡng!");
    alert = true;
  }
  
  if (data.ph < 6.8 || data.ph > 9.0) {
    Serial.println("   ⚠ pH ngoài ngưỡng!");
    alert = true;
  }
  
  if (data.tds > 30000) {
    Serial.println("   ⚠ TDS quá cao!");
    alert = true;
  }
  
  if (data.turbidity > 80) {
    Serial.println("   ⚠ Độ đục quá cao!");
    alert = true;
  }
  
  if (!alert) {
    Serial.println("   ✓ Tất cả thông số OK");
  }
  Serial.println();
}

// ============================================================================
// GỬI LÊN SUPABASE
// ============================================================================
void sendToSupabase(SensorData data) {
  Serial.println("☁ Đang gửi lên Supabase...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ Không có WiFi!");
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
  
  Serial.println("📤 Payload: " + jsonString);
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    Serial.printf("✓ Gửi thành công! Code: %d\n", httpCode);
    String response = http.getString();
    if (response.length() > 0) {
      Serial.println("   Response: " + response);
    }
  } else {
    Serial.printf("✗ Gửi thất bại! Error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

/*
 * ============================================================================
 * SƠ ĐỒ KẾT NỐI THEO BEARK BOARD
 * ============================================================================
 * 
 * [NGUỒN ACQUY 12V]
 *     │
 *     ├─── [Hạ áp 12V→5V] ──→ ESP32 + Beark Board
 *     └─── [Hạ áp 12V→5V] ──→ Relay 4 kênh VCC
 * 
 * [ESP32] ←──→ [BEARK BOARD] ←──→ [RELAY 4 KÊNH] ──→ [CẢM BIẾN]
 * 
 * CẤU HÌNH CHÂN THEO SƠ ĐỒ:
 * ┌────────────────────────────────────────────────────────┐
 * │ ESP32    → Beark Board → Relay → Cảm biến             │
 * ├────────────────────────────────────────────────────────┤
 * │ GPIO32   → DS18B20 Data (không qua relay)              │
 * │ GPIO33   → pH Analog Input                             │
 * │ GPIO34   → Turbidity Analog Input                      │
 * │ GPIO35   → TDS Analog Input                            │
 * ├────────────────────────────────────────────────────────┤
 * │ GPIO17   → Beark → Relay 1 IN → VCC CB Nhiệt độ       │
 * │ GPIO16   → Beark → Relay 2 IN → VCC CB pH              │
 * │ GPIO0    → Beark → Relay 3 IN → VCC CB TDS             │
 * │ GPIO15   → Beark → Relay 4 IN → VCC CB Turbidity       │
 * └────────────────────────────────────────────────────────┘
 * 
 * LƯU Ý:
 * - Module relay 4 kênh: LOW = BẬT (đèn sáng), HIGH = TẮT (đèn tắt)
 * - Khi chờ đo: TẤT CẢ 4 ĐÈN RELAY PHẢI TẮT
 * - Khi đo: Chỉ 1 đèn sáng từng lúc
 * - Tiết kiệm điện: ~97% so với để cảm biến BẬT liên tục
 * ============================================================================
 */