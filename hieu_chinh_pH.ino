/*
 * Hiệu chỉnh cảm biến pH DFRobot SEN0161-V2, hiệu chỉnh hai điểm.
 *
 * Cách làm:
 *   1. Ngâm probe vào dung dịch chuẩn pH 7.0, đợi khoảng 2 phút cho số ổn
 *      định, đọc điện áp trên Serial rồi điền vào VOLTAGE_PH7.
 *   2. Rửa probe bằng nước cất, lau khô.
 *   3. Làm lại với dung dịch pH 4.0, điền vào VOLTAGE_PH4.
 *   4. Nạp lại chương trình chính với hai giá trị vừa đo.
 *
 * Hai giá trị mặc định bên dưới chỉ là số tạm, phải đo lại trên đúng probe
 * đang dùng thì kết quả mới đúng.
 */

// Thông số hiệu chỉnh, cập nhật sau khi đo
// Hai số này phải đo lại trên probe thật, để nguyên là sai.
float VOLTAGE_PH7 = 1.50;  // V đo tại pH 7.0
float VOLTAGE_PH4 = 2.03;  // V đo tại pH 4.0

// Thông số chuẩn từ datasheet
const float PH_REFERENCE_7 = 7.0;
const float PH_REFERENCE_4 = 4.0;
const float ADC_RESOLUTION = 4095.0;  // ESP32 12-bit ADC
const float ADC_VREF = 3.3;           // Điện áp tham chiếu ESP32

// Hệ số tính từ hiệu chỉnh (tự động tính)
float acidSlope = 0.0;    // Độ dốc đường chuẩn
float neutralVoltage = 0.0; // Điện áp tại pH trung tính

// Tính hệ số hiệu chỉnh, gọi một lần trong setup()
void calibratePH() {
  Serial.println("HIỆU CHỈNH CẢM BIẾN pH SEN0161-V2");
  
  // Kiểm tra xem đã hiệu chỉnh chưa
  if (VOLTAGE_PH7 == 1.50 && VOLTAGE_PH4 == 2.03) {
    Serial.println("CẢNH BÁO: Đang dùng giá trị mặc định!");
    Serial.println("Bạn cần hiệu chỉnh bằng dung dịch chuẩn");
    Serial.println("pH 4.0 và pH 7.0 để có độ chính xác cao");
    Serial.println();
  }
  
  // Tính hệ số từ 2 điểm hiệu chỉnh
  acidSlope = (PH_REFERENCE_7 - PH_REFERENCE_4) / (VOLTAGE_PH7 - VOLTAGE_PH4);
  neutralVoltage = VOLTAGE_PH7;
  
  Serial.println("THÔNG SỐ HIỆU CHỈNH:");
  Serial.printf("Điện áp tại pH 7.0: %.3f V \n", VOLTAGE_PH7);
  Serial.printf("Điện áp tại pH 4.0: %.3f V \n", VOLTAGE_PH4);
  Serial.printf("Độ dốc (Slope): %.3f pH/V \n", acidSlope);
  Serial.printf("Điện áp trung tính: %.3f V \n", neutralVoltage);
  
  // Kiểm tra tính hợp lệ
  if (abs(acidSlope) < 2.0 || abs(acidSlope) > 4.5) {
    Serial.println("Cảnh báo: Độ dốc bất thường!");
    Serial.println("Giá trị chuẩn: -3.0 đến -3.5 pH/V");
    Serial.println("→ Kiểm tra lại hiệu chỉnh");
  } else {
    Serial.println("Hiệu chỉnh hợp lệ!");
  }
  Serial.println();
}

// Đọc pH, có hiệu chỉnh và bù nhiệt độ
float readPH_Calibrated(float temperature = 25.0) {
  // Đọc ADC (trung bình 10 mẫu)
  int adcSum = 0;
  for (int i = 0; i < 10; i++) {
    adcSum += analogRead(PIN_PH);
    delay(10);
  }
  float adcValue = adcSum / 10.0;
  
  // Chuyển đổi ADC sang điện áp
  float voltage = adcValue * (ADC_VREF / ADC_RESOLUTION);
  
  // Tính pH từ công thức hiệu chỉnh 2 điểm
  float phValue = PH_REFERENCE_7 + acidSlope * (voltage - neutralVoltage);
  
  // Bù nhiệt độ (pH thay đổi ~0.003 đơn vị/°C)
  float tempCompensation = 0.003 * (temperature - 25.0);
  phValue = phValue - tempCompensation;
  
  // Giới hạn trong khoảng hợp lệ
  phValue = constrain(phValue, 0.0, 14.0);
  
  // Hiển thị chi tiết
  Serial.printf("ADC Raw: %.0f / 4095\n", adcValue);
  Serial.printf("Voltage: %.3f V\n", voltage);
  Serial.printf("pH (uncalibrated): %.2f\n", 
                PH_REFERENCE_7 + acidSlope * (voltage - neutralVoltage));
  Serial.printf("Temperature: %.1f °C\n", temperature);
  Serial.printf("Temp compensation: %.3f pH\n", tempCompensation);
  Serial.printf("pH (calibrated): %.2f\n", phValue);
  
  return phValue;
}

// Chế độ đo tay để dò điện áp, gọi trong loop() khi cần hiệu chỉnh
void manualCalibrationMode() {
  Serial.println("CHẾ ĐỘ HIỆU CHỈNH pH MANUAL");
  Serial.println("Hướng dẫn:");
  Serial.println("1. Ngâm probe vào dung dịch pH 7.0");
  Serial.println("2. Đợi 2 phút, ghi lại điện áp");
  Serial.println("3. Rửa probe, ngâm vào pH 4.0");
  Serial.println("4. Đợi 2 phút, ghi lại điện áp");
  Serial.println("5. Cập nhật VOLTAGE_PH7 và VOLTAGE_PH4");
  
  for (int i = 0; i < 20; i++) {  // Đọc 20 lần
    // Đọc ADC
    int adcSum = 0;
    for (int j = 0; j < 10; j++) {
      adcSum += analogRead(PIN_PH);
      delay(10);
    }
    float adcValue = adcSum / 10.0;
    float voltage = adcValue * (ADC_VREF / ADC_RESOLUTION);
    
    Serial.printf("Mẫu %2d: ADC=%4.0f, Voltage=%.3fV\n", 
                  i+1, adcValue, voltage);
    delay(1000);
  }
  
  Serial.println("\n→ Ghi lại giá trị điện áp trung bình");
  Serial.println("→ Cập nhật vào VOLTAGE_PH7 hoặc VOLTAGE_PH4\n");
}

// Thay hàm readPH() cũ bằng hàm mới
// TRONG SETUP(), THÊM DÒNG:
// calibratePH();

// TRONG readAllSensors(), THAY ĐỔI:
// float temp = readTemperature();  // Đọc nhiệt độ trước
// data.ph = readPH_Calibrated(temp);  // Truyền nhiệt độ vào

/*
 * ============================================================================
 * HƯỚNG DẪN SỬ DỤNG CHI TIẾT
 * ============================================================================
 * 
 * BƯỚC 1: CHUẨN BỊ
 * - Chuẩn bị 2 cốc dung dịch chuẩn pH 4.0 và pH 7.0 (đi kèm sản phẩm)
 * - Chuẩn bị nước cất để rửa probe
 * - Giấy thấm mềm để lau khô probe
 * 
 * BƯỚC 2: HIỆU CHỈNH ĐIỂM THỨ NHẤT (pH 7.0)
 * 1. Upload code với chế độ manualCalibrationMode() trong loop
 * 2. Ngâm probe vào dung dịch pH 7.0
 * 3. Đợi 2 phút cho ổn định
 * 4. Quan sát Serial Monitor, ghi lại giá trị điện áp trung bình
 * 5. Cập nhật giá trị vào biến VOLTAGE_PH7
 * 
 * BƯỚC 3: HIỆU CHỈNH ĐIỂM THỨ HAI (pH 4.0)
 * 1. Rửa probe bằng nước cất, lau khô nhẹ
 * 2. Ngâm probe vào dung dịch pH 4.0
 * 3. Đợi 2 phút cho ổn định
 * 4. Ghi lại giá trị điện áp trung bình
 * 5. Cập nhật giá trị vào biến VOLTAGE_PH4
 * 
 * BƯỚC 4: HOÀN TẤT
 * 1. Upload lại code với 2 giá trị điện áp mới
 * 2. Gọi calibratePH() trong setup()
 * 3. Kiểm tra độ chính xác bằng dung dịch chuẩn
 * 
 * LƯU Ý BẢO QUẢN:
 * - Luôn ngâm probe trong dung dịch bảo quản (pH 4.0) khi không dùng
 * - KHÔNG để probe khô, sẽ làm hỏng màng thủy tinh
 * - Hiệu chỉnh lại mỗi 1-2 tháng hoặc khi phát hiện sai số
 * - Thay probe sau 6-12 tháng tùy tần suất sử dụng
 * 
 * KIỂM TRA SAI SỐ:
 * - Sai số cho phép: ±0.1 pH @ 25°C (theo datasheet)
 * - Nếu sai số > ±0.2 pH → cần hiệu chỉnh lại
 * - Nếu vẫn sai → kiểm tra probe (có thể hỏng)
 * 
 * ============================================================================
 */