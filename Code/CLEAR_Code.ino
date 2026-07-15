#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Stepper.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// WiFi Credentials
const char* ssid = "STYX";
const char* password = "1234567890";

// Telegram Bot Credentials
const char* botToken = "7717979364:AAEFSyDa79EGCV_NcQlhJrM1hfojfdwY21E";
const char* chatID = "1435322989";

// Telegram Setup
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  

#define MQ7_PIN       34  // MQ-7 Analog Output
#define RELAY_PIN     32  // Relay for ventilation
#define BUZZER_PIN    25  // Buzzer for CO warning

#define STEPS_PER_REV 2048  
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Stepper stepper(STEPS_PER_REV, IN1, IN3, IN2, IN4);

bool window_half_open = false;
bool window_fully_open = false;
bool warning_sent = false;
bool danger_sent = false;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  stepper.setSpeed(10);  

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi!");
  client.setInsecure();  // Ignore SSL errors for Telegram
}

void sendTelegramMessage(String message) {
  bot.sendMessage(chatID, message, "");
}

void loop() {
  int sensorValue = analogRead(MQ7_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  float co_ppm = voltage * 100;

  Serial.print("CO Level: ");
  Serial.print(co_ppm);
  Serial.println(" PPM");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("CO Level: ");
  display.print(co_ppm);
  display.println(" PPM");

  if (co_ppm < 50) {  
    Serial.println("✅ Safe - No action needed");
    display.println("Status: SAFE");
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    if (window_half_open || window_fully_open) {
      Serial.println("⬇️ Closing window...");
      stepper.step(-STEPS_PER_REV / 2);
      window_half_open = false;
      window_fully_open = false;
    }

    warning_sent = false;
    danger_sent = false;

  } else if (co_ppm >= 50 && co_ppm <= 150) {  
    Serial.println("⚠️ WARNING: Ventilation ON, Half-Opening Window");
    display.println("Status: WARNING!");
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    if (!window_half_open) {
      Serial.println("🔄 Opening window halfway...");
      stepper.step(STEPS_PER_REV / 4);
      window_half_open = true;
      window_fully_open = false;
    }

    if (!warning_sent) {
      sendTelegramMessage("⚠️ WARNING: CO Levels are rising! Ventilation activated.");
      warning_sent = true;
      danger_sent = false;
    }

  } else {  
    Serial.println("🚨 DANGER: Opening window fully!");
    display.println("Status: DANGER!");
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);

    if (!window_fully_open) {
      Serial.println("🔄 Opening window fully...");
      stepper.step(STEPS_PER_REV / 4);
      window_fully_open = true;
    }

    if (!danger_sent) {
      sendTelegramMessage("🚨 DANGER: HIGH CO DETECTED! Window fully opened, ventilation ON.");
      danger_sent = true;
    }
  }

  display.display();
  delay(2000);
}
