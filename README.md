# AquaOracle: firmware ESP32

Firmware cho node cảm biến của hệ thống AquaOracle: đo chất lượng nước ao nuôi
thuỷ sản rồi đẩy dữ liệu lên Supabase.

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

Tất cả được đặt trong hộp nhựa chống nước, nước được dẫn qua buồng đo bằng ống
PVC.

## Cách hoạt động

1. Khi khởi động: khởi tạo 4 cảm biến và kết nối Wi-Fi.
2. Khoảng 5 phút một lần: đọc analog nhiều lần rồi lấy trung bình để lọc nhiễu.
3. So với ngưỡng an toàn, nếu vượt thì bật cảnh báo.
4. Đóng gói thành JSON, POST lên Supabase REST API qua HTTPS và kiểm tra xem
   gửi có thành công không.

Mỗi bản ghi có `esp_id` để ứng dụng phân biệt được nhiều thiết bị trong cùng
một tài khoản.

## Cấu hình

```bash
cp secrets.example.h secrets.h   # rồi điền SSID, mật khẩu Wi-Fi và khoá Supabase
```

`secrets.h` nằm trong `.gitignore`.

Mở `Esp32_Program_AquaOracle.ino` bằng Arduino IDE, chọn board ESP32 Dev Module
rồi nạp chương trình.

## Ghi chú bảo mật

Các bản trước của repo này hardcode SSID, mật khẩu Wi-Fi và Supabase anon key
ngay trong file `.ino`, và đã bị đẩy lên repo public. Tôi đã viết lại lịch sử
git bằng `git filter-repo` để gỡ ra, đổi mật khẩu Wi-Fi và xoá Supabase project
cũ. Giờ cấu hình nằm riêng trong `secrets.h`.

## Trạng thái

Cuộc thi đã kết thúc. Mã nguồn để nguyên để tham khảo; muốn chạy lại thì cần
tạo một Supabase project mới.
