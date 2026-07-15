# -*- coding: utf-8 -*-
"""
===================================================================
  VOICE AI AGENT BRIDGE FOR ARDUINO UNO
===================================================================
  Hướng dẫn sử dụng:
    1. Cài đặt các thư viện Python:
       pip install SpeechRecognition pyaudio pyserial
       
    2. Cắm mạch Arduino Uno đã nạp code C++ vào máy tính qua cổng USB.
    
    3. Kiểm tra số cổng COM (Device Manager trên Windows, ví dụ COM3, COM4).
       Đổi biến PORT phía dưới cho đúng cổng COM của bạn.
       
    4. Chạy script này:
       python voice_agent_bridge.py
       
    5. Đọc các khẩu lệnh để điều khiển thiết bị:
       - "Bật đèn gaming" / "Tắt đèn led" / "Màu đỏ" / "Màu xanh"
       - "Bật quạt turbo" / "Tắt quạt tản nhiệt"
       - "Khóa cửa lại" / "Mở cửa phòng ngủ"
       - "Bật đèn ngủ" / "Tắt cozy lamp"
       - "Hát đi" (phát nhạc Rickroll qua còi Buzzer)
       - "Tắt nhạc" / "Im đi" (dừng phát nhạc)
===================================================================
"""

import speech_recognition as sr
import serial
import time
import sys

# Cấu hình cổng COM tương ứng của Arduino (ví dụ COM3, COM4 trên Windows hoặc /dev/ttyUSB0 trên Linux)
PORT = 'COM3' 
BAUD = 9600

# Thiết lập bảng mã tiếng Việt cho terminal hiển thị đúng
if sys.platform.startswith('win'):
    import ctypes
    # Đặt console output là UTF-8
    ctypes.windll.kernel32.SetConsoleOutputCP(65001)

print("-------------------------------------------------------------------")
print("  VOICE AI AGENT - KẾT NỐI ARDUINO UNO GAMING ROOM SETUP")
print("-------------------------------------------------------------------")

try:
    arduino = serial.Serial(PORT, BAUD, timeout=1)
    print(f"✔ Đã kết nối thành công với Arduino trên cổng: {PORT}")
    print("⏳ Đang đợi 2 giây để mạch Arduino reset...")
    time.sleep(2) 
    print("✔ Mạch sẵn sàng nhận lệnh.")
except Exception as e:
    print(f"❌ Lỗi kết nối cổng Serial: {e}")
    print("\nHướng dẫn sửa lỗi:")
    print("  1. Hãy cắm cáp USB nối Arduino Uno với máy tính.")
    print("  2. Mở Arduino IDE xem cổng COM là bao nhiêu (ví dụ COM3).")
    print(f"  3. Mở file 'voice_agent_bridge.py' này ra và sửa 'PORT = \"COM3\"' cho đúng cổng.")
    sys.exit(1)

def send_command(cmd_str):
    """Gửi lệnh định dạng CMD:... xuống Arduino qua Serial"""
    try:
        full_cmd = f"{cmd_str}\n"
        arduino.write(full_cmd.encode('utf-8'))
        print(f"➔ Đã gửi lệnh xuống mạch: {cmd_str}")
        
        # Chờ phản hồi từ Arduino để xác nhận (ACK)
        time.sleep(0.15)
        if arduino.in_waiting > 0:
            ack = arduino.readline().decode('utf-8').strip()
            print(f"⬅ Phản hồi từ Arduino: {ack}")
    except Exception as ex:
        print(f"❌ Lỗi khi gửi dữ liệu Serial: {ex}")

# Khởi tạo bộ thu âm và nhận diện giọng nói Google Speech
r = sr.Recognizer()
mic = sr.Microphone()

print("\n--- BẮT ĐẦU HOẠT ĐỘNG ---")
print("Vui lòng nói rõ ràng vào micro của máy tính.")
print("Nhấn Ctrl + C nếu muốn thoát chương trình.")
print("-------------------------------------------------------------------")

try:
    while True:
        with mic as source:
            # Tự động lọc tiếng ồn môi trường
            r.adjust_for_ambient_noise(source, duration=0.4)
            print("\n🗣️ Mời bạn nói...")
            try:
                # Nghe âm thanh từ microphone
                audio = r.listen(source, timeout=4, phrase_time_limit=4)
                print("⏳ Đang nhận diện tiếng Việt...")
                
                # Gọi Google API nhận dạng tiếng Việt miễn phí
                text = r.recognize_google(audio, language="vi-VN")
                print(f"🎙️ Bạn đã nói: '{text}'")
                
                clean_text = text.lower().strip()
                
                # Khai báo các từ khóa hỗ trợ
                def matches_words(text, keywords):
                    return any(kw in text for kw in keywords)

                is_turn_on = matches_words(clean_text, ["bật", "mở", "lên", "chạy", "sáng", "quay", "kích hoạt", "on"])
                is_turn_off = matches_words(clean_text, ["tắt", "ngắt", "dừng", "tát", "im", "off"])

                is_all = matches_words(clean_text, ["tất cả", "hết", "mọi thứ", "all", "cả phòng"])
                has_both_lights = ("cả hai" in clean_text and "đèn" in clean_text) or \
                                  (matches_words(clean_text, ["đèn ngủ", "cozy", "lamp", "ngủ"]) and matches_words(clean_text, ["đèn led", "led", "dải"]))

                is_ambiguous = not is_all and not has_both_lights and "đèn" in clean_text and not matches_words(clean_text, ["gaming", "led", "dải", "nền", "ngủ", "cozy", "lamp"])
                has_led = (matches_words(clean_text, ["đèn", "led", "dải"]) and not matches_words(clean_text, ["đèn ngủ", "cozy", "lamp"])) or clean_text == "led" or clean_text == "đèn"
                has_fan = matches_words(clean_text, ["quạt", "fan", "gió"])
                has_lock = matches_words(clean_text, ["khóa", "khoá", "đóng", "chốt", "sập", "cửa", "lock", "latch", "cổng"])
                has_lamp = matches_words(clean_text, ["đèn ngủ", "cozy", "lamp", "ngủ"])
                has_buzzer = matches_words(clean_text, ["hát", "nhạc", "rickroll", "âm nhạc", "còi", "chuông", "báo thức", "bzz"])

                # Phân tích cú pháp khẩu lệnh để gửi mã lệnh Serial tương ứng
                # Kiểm tra xem có yêu cầu hẹn giờ/lập lịch hay không (ví dụ: "lúc 18 giờ 30")
                import re
                time_match = re.search(r'lúc\s*(\d+)\s*giờ\s*(\d+)?', clean_text)
                if time_match:
                    hr = int(time_match.group(1))
                    mn = int(time_match.group(2)) if time_match.group(2) else 0
                    if 0 <= hr <= 23 and 0 <= mn <= 59:
                        action = 0 if is_turn_off else 1
                        
                        if is_all:
                            send_command(f"CMD:RUL:1:{hr:02d}:{mn:02d}:10:{action}")
                            time.sleep(0.15)
                            send_command(f"CMD:RUL:2:{hr:02d}:{mn:02d}:11:{action}")
                            time.sleep(0.15)
                            send_command(f"CMD:RUL:4:{hr:02d}:{mn:02d}:13:{action}")
                            print(f"✔ Đã lập lịch tự động: Bật tất cả thiết bị vào lúc {hr:02d}:{mn:02d}")
                            continue

                        if has_both_lights:
                            send_command(f"CMD:RUL:1:{hr:02d}:{mn:02d}:10:{action}")
                            time.sleep(0.15)
                            send_command(f"CMD:RUL:4:{hr:02d}:{mn:02d}:13:{action}")
                            print(f"✔ Đã lập lịch tự động: Bật cả hai đèn vào lúc {hr:02d}:{mn:02d}")
                            continue

                        if is_ambiguous:
                            print("❓ Bạn muốn hẹn giờ cho đèn LED gaming hay đèn ngủ?")
                            continue

                        if has_buzzer:
                            send_command(f"CMD:ALM:{hr:02d}:{mn:02d}")
                            print(f"✔ Đã đặt hẹn giờ lên nhạc lúc {hr:02d}:{mn:02d}")
                            continue
                            
                        pin = None
                        if has_lamp:
                            pin = 13
                        elif has_led:
                            pin = 10
                        elif has_fan:
                            pin = 11
                        elif has_lock:
                            pin = 12
                            action = 0 if matches_words(clean_text, ["mở", "bẻ", "ra"]) else 1

                        if pin is not None and action is not None:
                            slot = pin - 9 # Pin 10 -> slot 1 (Đèn), Pin 11 -> slot 2 (Quạt), etc.
                            send_command(f"CMD:RUL:{slot}:{hr:02d}:{mn:02d}:{pin}:{action}")
                            print(f"✔ Đã lập lịch tự động: Pin D{pin} -> {action} vào lúc {hr:02d}:{mn:02d}")
                            continue

                # 0. Kiểm tra combo thiết bị trước
                if is_all and is_turn_on:
                    send_command("CMD:LED:1")
                    time.sleep(0.15)
                    send_command("CMD:FAN:1")
                    time.sleep(0.15)
                    send_command("CMD:LMP:1")
                    print("✔ Đã bật tất cả thiết bị (đèn led, quạt, đèn ngủ).")
                elif is_all and is_turn_off:
                    send_command("CMD:LED:0")
                    time.sleep(0.15)
                    send_command("CMD:FAN:0")
                    time.sleep(0.15)
                    send_command("CMD:LMP:0")
                    time.sleep(0.15)
                    send_command("CMD:BZZ:0")
                    print("✔ Đã tắt tất cả thiết bị.")
                elif has_both_lights and is_turn_on:
                    send_command("CMD:LED:1")
                    time.sleep(0.15)
                    send_command("CMD:LMP:1")
                    print("✔ Đã bật cả hai đèn (led và đèn ngủ).")
                elif has_both_lights and is_turn_off:
                    send_command("CMD:LED:0")
                    time.sleep(0.15)
                    send_command("CMD:LMP:0")
                    print("✔ Đã tắt cả hai đèn (led và đèn ngủ).")
                
                # 1. Kiểm tra nhập nhèm đèn (Ambiguity check)
                elif is_ambiguous:
                    if is_turn_on:
                        print("❓ Bạn muốn bật đèn LED gaming hay đèn ngủ?")
                    elif is_turn_off:
                        print("❓ Bạn muốn tắt đèn LED gaming hay đèn ngủ?")
                    else:
                        print("❓ Bạn muốn điều khiển đèn LED gaming hay đèn ngủ?")
                
                # 1. Đèn LED Gaming
                elif has_led and is_turn_on:
                    send_command("CMD:LED:1")
                elif has_led and is_turn_off:
                    send_command("CMD:LED:0")
                elif has_led and ("màu đỏ" in clean_text or "đỏ" in clean_text):
                    send_command("CMD:LED:R")
                elif has_led and ("xanh lá" in clean_text or "lục" in clean_text or "xanh lá cây" in clean_text):
                    send_command("CMD:LED:G")
                elif has_led and ("xanh dương" in clean_text or "xanh lam" in clean_text or "xanh dương" in clean_text):
                    send_command("CMD:LED:B")
                elif has_led and ("màu tím" in clean_text or "tím" in clean_text):
                    send_command("CMD:LED:P")
                elif has_led and ("vũ trường" in clean_text or "party" in clean_text or "quẩy" in clean_text or "nháy" in clean_text):
                    send_command("CMD:LED:W")
                
                # 2. Quạt tản nhiệt PC
                elif has_fan and is_turn_on:
                    send_command("CMD:FAN:1")
                elif has_fan and is_turn_off:
                    send_command("CMD:FAN:0")
                
                # 3. Khóa cửa phòng ngủ
                elif has_lock and ("mở" in clean_text or "ra" in clean_text or "bẻ" in clean_text):
                    send_command("CMD:LCK:0")
                elif has_lock and ("khóa" in clean_text or "khoá" in clean_text or "đóng" in clean_text or "chốt" in clean_text or "sập" in clean_text or "lại" in clean_text):
                    send_command("CMD:LCK:1")
                
                # 4. Đèn ngủ Cozy
                elif has_lamp and is_turn_on:
                    send_command("CMD:LMP:1")
                elif has_lamp and is_turn_off:
                    send_command("CMD:LMP:0")
                
                # 5. Còi phát nhạc Rickroll
                elif has_buzzer and is_turn_on:
                    send_command("CMD:BZZ:R")
                elif "im" in clean_text or "tắt nhạc" in clean_text or "dừng" in clean_text or "dừng hát" in clean_text:
                    send_command("CMD:BZZ:0")
                else:
                    print("❓ Không tìm thấy từ khóa khớp với thiết bị. Thử lại nhé.")
                    
            except sr.WaitTimeoutError:
                # Không nghe thấy tiếng nói, lặp lại
                pass
            except sr.UnknownValueError:
                print("❌ Không nghe rõ từ ngữ. Vui lòng nói lại.")
            except Exception as ex:
                print(f"❌ Lỗi xử lý: {ex}")

except KeyboardInterrupt:
    print("\n👋 Đã dừng chương trình kết nối giọng nói. Hẹn gặp lại bạn!")
    if 'arduino' in locals() and arduino.is_open:
        arduino.close()
