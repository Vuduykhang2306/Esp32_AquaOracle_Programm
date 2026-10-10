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
    Serial.println("-> Kiểm tra lại hiệu chỉnh");
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
  
  Serial.println("\n-> Ghi lại giá trị điện áp trung bình");
  Serial.println("-> Cập nhật vào VOLTAGE_PH7 hoặc VOLTAGE_PH4\n");
}

// Cách dùng trong chương trình chính: thay readPH() cũ bằng hàm ở trên.
// Trong setup(), thêm:
//   calibratePH();
// Trong readAllSensors(), đọc nhiệt độ trước rồi truyền vào:
//   float temp = readTemperature();
//   data.ph = readPH_Calibrated(temp);

/*
 * Ghi chú thêm:
 *   - Dung dịch chuẩn pH 4.0 và 7.0 thường đi kèm cảm biến. Chuẩn bị thêm
 *     nước cất để rửa probe và giấy mềm để lau.
 *   - Lúc dò điện áp thì gọi manualCalibrationMode() trong loop(). Mỗi dung
 *     dịch đợi khoảng 2 phút rồi lấy điện áp trung bình trên Serial Monitor.
 *   - Có hai giá trị rồi thì nạp lại code, gọi calibratePH() trong setup()
 *     và thử lại bằng dung dịch chuẩn để xem sai số.
 *   - Khi không dùng, ngâm probe trong dung dịch bảo quản (pH 4.0). Đừng để
 *     probe bị khô, sẽ hỏng màng thuỷ tinh.
 *   - Theo datasheet, sai số cho phép là ±0.1 pH ở 25°C. Lệch quá ±0.2 pH
 *     thì hiệu chỉnh lại, vẫn lệch thì có thể probe đã hỏng.
 *   - Nên hiệu chỉnh lại sau 1-2 tháng. Probe thường dùng được 6-12 tháng
 *     tuỳ tần suất đo.
 */