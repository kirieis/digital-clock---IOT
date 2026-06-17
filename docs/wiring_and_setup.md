# Hướng Dẫn Kết Nối & Cài Đặt - Digital Clock

## 📌 Sơ Đồ Kết Nối Chi Tiết

```
                          ┌─────────────────────┐
                          │    ARDUINO UNO R3    │
                          │                     │
    ┌──────────┐          │  5V  ───────────────┼──── VCC (DS3231 + LCD)
    │ DS3231   │          │  GND ───────────────┼──── GND (DS3231 + LCD + Buzzer + 3 Nút)
    │ RTC      │          │  A4  (SDA) ─────────┼──── SDA (DS3231 + LCD)
    │  VCC ────┼──── 5V   │  A5  (SCL) ─────────┼──── SCL (DS3231 + LCD)
    │  GND ────┼──── GND  │                     │
    │  SDA ────┼──── A4   │  D2  ───────────────┼──── Nút MODE ──── GND
    │  SCL ────┼──── A5   │  D3  ───────────────┼──── Nút UP   ──── GND
    └──────────┘          │  D4  ───────────────┼──── Nút DOWN ──── GND
                          │                     │
    ┌──────────┐          │  D8  ───────────────┼──── Buzzer (+)
    │ LCD I2C  │          │                     │      Buzzer (-) ──── GND
    │  VCC ────┼──── 5V   └─────────────────────┘
    │  GND ────┼──── GND
    │  SDA ────┼──── A4
    │  SCL ────┼──── A5
    └──────────┘
```

### Bảng tổng hợp kết nối

| Chân Arduino | Kết nối tới | Ghi chú |
|:---:|---|---|
| **5V** | DS3231 VCC, LCD VCC | Nguồn 5V chung |
| **GND** | DS3231 GND, LCD GND, Buzzer (-), 3 nút (1 chân) | Mass chung |
| **A4 (SDA)** | DS3231 SDA, LCD SDA | Bus I2C - Data |
| **A5 (SCL)** | DS3231 SCL, LCD SCL | Bus I2C - Clock |
| **D2** | Nút MODE (chân kia → GND) | `INPUT_PULLUP` |
| **D3** | Nút UP (chân kia → GND) | `INPUT_PULLUP` |
| **D4** | Nút DOWN (chân kia → GND) | `INPUT_PULLUP` |
| **D8** | Buzzer chân (+) | Chân (-) → GND |

### Lưu ý khi nối dây
- **DS3231 và LCD dùng chung SDA, SCL** → Nối song song (parallel), không nối nối tiếp (series).
- **Nút nhấn**: Mỗi nút chỉ cần 2 dây: 1 dây → chân Digital, 1 dây → GND. **KHÔNG cần điện trở ngoài** (Arduino dùng điện trở kéo lên nội bộ `INPUT_PULLUP`).
- **Buzzer**: Kiểm tra chân (+) và (-). Chân (+) thường có dấu hoặc chân dài hơn.

---

## 📦 Cài Đặt Thư Viện (Arduino IDE)

### Bước 1: Mở Library Manager
`Sketch` → `Include Library` → `Manage Libraries...`

### Bước 2: Cài 2 thư viện

| Thư viện | Tác giả | Cách tìm |
|----------|---------|----------|
| **RTClib** | Adafruit | Tìm `RTClib`, chọn bản của **Adafruit** |
| **LiquidCrystal I2C** | Frank de Brabander | Tìm `LiquidCrystal I2C`, chọn bản của **Frank de Brabander** |

### Bước 3: Kiểm tra địa chỉ I2C (Nếu LCD không hiện)
Nếu LCD không hiển thị gì sau khi upload code, có thể sai địa chỉ I2C.

Upload đoạn code quét I2C này để tìm địa chỉ đúng:

```cpp
#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("I2C Scanner...");
  
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Done.");
}

void loop() {}
```

Kết quả mong đợi:
```
Found: 0x27   ← Địa chỉ LCD (hoặc 0x3F)
Found: 0x68   ← Địa chỉ DS3231
```

Nếu LCD là `0x3F`, sửa dòng `#define LCD_ADDR 0x27` thành `#define LCD_ADDR 0x3F` trong file code chính.

---

## 🎮 Hướng Dẫn Sử Dụng

### Chế độ xem giờ (mặc định)
- Dòng 1: `Time: HH:MM:SS`
- Dòng 2: Xen kẽ `Date: DD/MM/YYYY` và `Alarm:HH:MM  ON`

### Các nút
| Nút | Chế độ xem giờ | Chế độ chỉnh |
|-----|----------------|--------------|
| **MODE** (D2) | Vào chỉnh giờ | Chuyển sang mục tiếp theo |
| **UP** (D3) | Bật/tắt báo thức | Tăng giá trị +1 |
| **DOWN** (D4) | Bật/tắt báo thức | Giảm giá trị -1 |

### Thứ tự chỉnh: 
`Giờ` → `Phút` → `Giờ báo thức` → `Phút báo thức` → `Quay về hiển thị`

### Khi chuông kêu
Nhấn **bất kỳ nút nào** để tắt chuông.
