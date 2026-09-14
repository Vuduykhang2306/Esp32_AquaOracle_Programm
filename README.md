# AquaOracle — firmware ESP32

Firmware cho node cảm biến của hệ thống AquaOracle: đo chất lượng nước ao nuôi
thuỷ sản và đẩy dữ liệu lên Supabase.

Ứng dụng di động đọc dữ liệu này: [aquaoracle-app](https://github.com/Vuduykhang2306/aquaoracle-app)

## Phần cứng

| Thành phần | Model |
|---|---|
| Vi điều khiển | ESP32-WROOM |
| Nhiệt độ | DS18B20 (1-Wire) |
| pH | Analog pH Meter Kit V2 |
| TDS | cảm biến TDS analog |
| Độ đục | cảm biến độ đục analog |
| Nguồn | ắc quy 12V, có thể ghép pin mặt trời |

Toàn bộ đặt trong hộp nhựa chống nước, ống PVC dẫn nước qua buồng đo.

## Cách hoạt động

1. Khởi động, khởi tạo 4 cảm biến và kết nối Wi-Fi.
2. Cứ khoảng 5 phút: đọc analog nhiều lần rồi lấy trung bình để lọc nhiễu.
3. So sánh với ngưỡng an toàn, bật cảnh báo nếu vượt.
4. Đóng gói JSON, POST lên Supabase REST API qua HTTPS, kiểm tra kết quả gửi.

Mỗi bản ghi mang `esp_id` để ứng dụng phân biệt được nhiều thiết bị trong cùng
một tài khoản.

## Cấu hình

```bash
cp secrets.example.h secrets.h   # rồi điền SSID, mật khẩu Wi-Fi và khoá Supabase
```

`secrets.h` nằm trong `.gitignore`.

Mở `Esp32_Program_AquaOracle.ino` bằng Arduino IDE, chọn board ESP32 Dev Module,
nạp chương trình.

## Ghi chú bảo mật

Các bản trước của repo này có hardcode SSID và mật khẩu Wi-Fi cùng Supabase anon
key ngay trong file `.ino`, và đã bị đẩy lên repo public. Lịch sử git đã được
viết lại bằng `git filter-repo` để gỡ bỏ, mật khẩu mạng đã được đổi, và Supabase
project cũ đã bị xoá. Cấu hình hiện tách hẳn sang `secrets.h`.

## Trạng thái

Dự án thi đã kết thúc. Mã nguồn giữ nguyên để tham chiếu; muốn chạy lại cần một
Supabase project mới.
