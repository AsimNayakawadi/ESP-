#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// OLED pins
#define OLED_SDA 16
#define OLED_SCL 17

// RFID config
#define RST_PIN 22
#define SS_PIN  5
MFRC522 rfid(SS_PIN, RST_PIN);

// Buzzer and LED
#define BUZZER_PIN 4
#define LED_ERROR 27

// Wi-Fi credentials
const char* ssid     = "G2-ESP";
const char* password = "cFdwpYxQocnzeD48";

// Server endpoint
const char* server = "141.18.1.124 ";
const int stationNumber = 1;

void buzz() {
  tone(BUZZER_PIN, 1000);
  delay(300);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  digitalWrite(LED_ERROR, LOW);

  // DIP switch removed for simplicity
  // OLED
  pinMode(OLED_SDA, INPUT_PULLUP);
  pinMode(OLED_SCL, INPUT_PULLUP);
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED failed");
    digitalWrite(LED_ERROR, HIGH);
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("Connecting WiFi...");
  display.display();

  // RFID
  SPI.begin();
  rfid.PCD_Init();

  // Wi-Fi
  WiFi.disconnect(true, true);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED || WiFi.localIP().toString() == "0.0.0.0") && retries < 15) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED || WiFi.localIP().toString() == "0.0.0.0") {
    Serial.println("❌ Wi-Fi failed");
    digitalWrite(LED_ERROR, HIGH);
    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(1);
    display.println("WiFi FAILED!");
    display.display();
    while (true);
  }

  Serial.println("\n✅ Wi-Fi connected");
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Scan your card...");
  display.display();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  // UID
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toLowerCase();
  Serial.println("Card: " + uidStr);

  // HTTP PUT
  HTTPClient http;
  http.begin(server);
  http.addHeader("Content-Type", "application/json");

  String json = "{\"stationNumber\":" + String(stationNumber) + ",\"cardId\":\"" + uidStr + "\"}";
  int httpCode = http.PUT(json);

  bool accessGranted = false;

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("Server Response: " + response);
    if (response.indexOf("\"success\":true") > 0) {
      accessGranted = true;
    }
  } else {
    Serial.println("HTTP ERROR");
    digitalWrite(LED_ERROR, HIGH);
  }

  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(2);

  if (accessGranted) {
    display.println("ACCESS");
    display.println("GRANTED");
    buzz();
  }

  display.display();
  delay(3000);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) {
    delay(100);
  }

  digitalWrite(LED_ERROR, LOW);
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Scan your card...");
  display.display();
}
