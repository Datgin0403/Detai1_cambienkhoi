// ===== 1. KHAI BÁO BLYNK TEMPLATE (ĐÃ TẠM TẮT) =====
// #define BLYNK_TEMPLATE_ID "DÁN_TEMPLATE_ID_VÀO_ĐÂY"
// #define BLYNK_TEMPLATE_NAME "DÁN_TEMPLATE_NAME_VÀO_ĐÂY"
// #define BLYNK_AUTH_TOKEN "DÁN_AUTH_TOKEN_VÀO_ĐÂY"
// ==================================================

// ===== 2. THÊM CÁC THƯ VIỆN =====
// #define BLYNK_PRINT Serial // Tạm tắt
// #include <ESP8266WiFi.h>          // Tạm tắt
// #include <BlynkSimpleEsp8266.h>   // Tạm tắt
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// ==================================================

// ========== PIN DEFINITIONS(ESP8266)==========
#define MQ2_PIN A0       // Đọc giá trị analog từ cảm biến gas

// Chân I2C (SDA, SCL) cho LCD trên NodeMCU
#define SDA_PIN D2       // I2C Data cho LCD
#define SCL_PIN D1       // I2C Clock cho LCD

#define BUZZER_PIN D5    // Điều khiển còi
#define LED_RED D0       // Đèn LED cảnh báo
#define BUTTON_PIN D6    // Nút tắt tiếng

// Ngưỡng cảm biến gas
#define SAFE_THRESHOLD 150     
#define WARNING_THRESHOLD 400 

// Tốc độ nháy đèn
#define FAST_BLINK 300      // Nguy hiểm: nháy nhanh
#define SLOW_BLINK 800      // Cảnh báo: nháy chậm

#define SILENCE_DURATION 30000
#define DEBOUNCE_DELAY 500
#define UPDATE_INTERVAL 200
#define GAS_CHANGE_THRESHOLD 5

LiquidCrystal_I2C lcd(0x27, 16, 2); // Khởi tạo LCD

// Biến toàn cục
int gasValue = 0;
int previousGasValue = -1;
bool buzzerActive = false;
bool buzzerSilenced = false;
unsigned long silenceStartTime = 0;
unsigned long lastButtonPress = 0;
String previousStatus = "";
unsigned long lastBlink = 0;
bool ledState = false;
// bool daGuiThongBaoBlynk = false; // Tạm tắt
String cachedLine1 = "";
String cachedLine2 = "";
// ========== FUNCTION PROTOTYPES ==========
void displayStartup();
void updateLCD(String line1, String line2, bool force = false);
void updateGasDisplay();
void handleButton();
void checkSilenceTimeout();
void processGasLevel();
void safeMode();
void warningMode();
void dangerMode();
void blinkLED(int blinkInterval);
// ========== SETUP ==========
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Dùng điện trở kéo nội

  Serial.begin(9600);
  Serial.println("=== HỆ THỐNG CẢM BIẾN KHÍ GAS - ESP8266 (OFFLINE) ===");

  // Khởi tạo LCD
  Wire.begin(SDA_PIN, SCL_PIN); // Chỉ định chân I2C cho ESP8266
  lcd.init();
  lcd.backlight();

  displayStartup();
  
  updateLCD("He thong online", "Cho cam bien...", true);
  Serial.println("Hệ thống đã sẵn sàng, đang chờ dữ liệu cảm biến...");
  delay(1500); // Chờ 1.5s để đọc
  lcd.clear();
  cachedLine1 = "";
  cachedLine2 = "";
}

//Vòng lặp chính 
void loop() {
  // Blynk.run(); // Tạm tắt
  gasValue = analogRead(MQ2_PIN); // 1. Đọc cảm biến
  handleButton(); // 2. Xử lý nút bấm
  checkSilenceTimeout(); // 3. Kiểm tra hết hạn tắt tiếng
  updateGasDisplay(); // 4. Cập nhật LCD
  processGasLevel(); // 5. Xử lý trạng thái
  if (previousStatus == "Warning") {
    blinkLED(SLOW_BLINK);
  } else if (previousStatus == "Danger") {
    blinkLED(FAST_BLINK);
  }
}

//Hiển thị thông báo hệ thống
void displayStartup() {
  updateLCD("Cam bien khi gas", "Khoi tao...", true);
  Serial.println("Đang khởi tạo cảm biến khí gas...");
  delay(1000); 
  lcd.clear();
  cachedLine1 = "";
  cachedLine2 = "";
}

//Cập nhật trên màn hình LCD
void updateLCD(String line1, String line2, bool force) {
  if (force || line1 != cachedLine1) {
    lcd.setCursor(0, 0);
    lcd.print(line1);
    for (int i = line1.length(); i < 16; i++) {
      lcd.print(" ");
    }
    cachedLine1 = line1;
  }
  if (force || line2 != cachedLine2) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
    for (int i = line2.length(); i < 16; i++) {
      lcd.print(" ");
    }
    cachedLine2 = line2;
  }
}
//Cập nhật giá trị khí gas lên LCD và Serial Monitor
void updateGasDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < UPDATE_INTERVAL) {
    return; 
  }
  lastUpdate = millis();

  // Chỉ cập nhật khi giá trị thay đổi
  if (abs(gasValue - previousGasValue) > GAS_CHANGE_THRESHOLD) {
    String gasLine = "Gas: " + String(gasValue);
    updateLCD(gasLine, cachedLine2);
    previousGasValue = gasValue;

    // Blynk.virtualWrite(V0, gasValue); // Tạm tắt

    Serial.print("Nồng độ khí gas: ");
    Serial.print(gasValue);
    Serial.print(" | Trạng thái: ");
    Serial.println(previousStatus);
  }
}

//Xử lí nút bấm
void handleButton() {
  int buttonState = digitalRead(BUTTON_PIN);
  unsigned long currentTime = millis();

  if (buttonState == LOW && (currentTime - lastButtonPress > DEBOUNCE_DELAY)) {
    lastButtonPress = currentTime;
    buzzerSilenced = true;
    silenceStartTime = currentTime;

    if (buzzerActive) {
      noTone(BUZZER_PIN);
      buzzerActive = false;
    }
    updateLCD(cachedLine1, "Tat tieng 30s", true);
    Serial.println(">>> NÚT BẤM - Đã tắt tiếng còi trong 30 giây");
  }
}

// Đếm ngược thời gian tắt tiếng (30 giây)
void checkSilenceTimeout() {
  if (buzzerSilenced) {
    unsigned long elapsed = millis() - silenceStartTime;

    if (elapsed < SILENCE_DURATION) {
      int secondsLeft = (SILENCE_DURATION - elapsed) / 1000;
      String muteMsg = "Tat tieng " + String(secondsLeft) + "s";
      updateLCD(cachedLine1, muteMsg);
    } else {
      buzzerSilenced = false;
      Serial.println(">>> Hết thời gian tắt tiếng - Còi báo động đã được kích hoạt lại");
    }
  }
}

// Điều khiển nháy đèn
void blinkLED(int blinkInterval) {
  if (millis() - lastBlink > blinkInterval) {
    ledState = !ledState;
    digitalWrite(LED_RED, ledState);
    lastBlink = millis();
  }
}

// Xử lí trạng thái khí gas
void processGasLevel() {
  String currentStatus;

  if (gasValue < SAFE_THRESHOLD) {
    currentStatus = "Safe";
    if (previousStatus != currentStatus) {
      safeMode();
      previousStatus = currentStatus;
    }
  } 
  else if (gasValue < WARNING_THRESHOLD) {
    currentStatus = "Warning";
    if (previousStatus != currentStatus) {
      warningMode();
      previousStatus = currentStatus;
    }
  } 
  else {
    currentStatus = "Danger";
    if (previousStatus != currentStatus) {
      dangerMode();
      previousStatus = currentStatus;
    }
  }
}

// Safe Mode (LED OFF)
void safeMode() {
  digitalWrite(LED_RED, LOW); // Tắt đèn

  if (buzzerActive) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  }
  buzzerSilenced = false;
  // daGuiThongBaoBlynk = false; // Tạm tắt

  updateLCD(cachedLine1, "KK: An toan");
}

// Warning Mode (LED Slow Blink)
void warningMode() {
  if (buzzerActive) {
    noTone(BUZZER_PIN); 
    buzzerActive = false;
  }

  // daGuiThongBaoBlynk = false; // Tạm tắt

  updateLCD(cachedLine1, "KK: Canh bao");
}

// Danger Mode (LED Fast Blink and BUZZER ON)
void dangerMode() {
  if (!buzzerSilenced) {
    if (!buzzerActive) {
      tone(BUZZER_PIN, 1000);
      buzzerActive = true;
      Serial.println(">>> Còi báo động: BẬT!");
    }
    // Gửi thông báo Blynk (Tạm tắt)
    /*
    if (!daGuiThongBaoBlynk) {
      String msg = "CANH BAO NGUY HIEM! Nong do gas: " + String(gasValue);
      Blynk.notify(msg);
      Serial.println(">>> BLYNK NOTIFICATION SENT!");
      daGuiThongBaoBlynk = true; 
    }
    */
  } else {
    if (buzzerActive) {
      noTone(BUZZER_PIN);
      buzzerActive = false;
    }
  }

  if (!buzzerSilenced) {
    updateLCD(cachedLine1, "!! NGUY HIEM !!");
  }
}