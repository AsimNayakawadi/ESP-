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

// LED pins
#define LED_SYS_READY 12
#define LED_CONNECTED 14
#define LED_ERROR     27

// I2C pins
#define OLED_SDA 16
#define OLED_SCL 17

// RFID config
#define RST_PIN 22
#define SS_PIN  5
MFRC522 rfid(SS_PIN, RST_PIN);

// Buzzer
#define BUZZER_PIN 4

// Wi-Fi credentials
const char* ssid     = "Asim's iPhone";
const char* password = "rockon38";

// Server info
const char* server = "http://campusdayapp.germanywestcentral.cloudapp.azure.com/services/station/check";
const int stationNumber = 1;

// Buzz helper
void buzz(int duration = 700) {
  tone(BUZZER_PIN, 1000);
  delay(duration);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_SYS_READY, OUTPUT);
  pinMode(LED_CONNECTED, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  digitalWrite(LED_SYS_READY, HIGH);     // System ready
  digitalWrite(LED_CONNECTED, LOW);
  digitalWrite(LED_ERROR, LOW);

  pinMode(OLED_SDA, INPUT_PULLUP);
  pinMode(OLED_SCL, INPUT_PULLUP);
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    digitalWrite(LED_ERROR, HIGH);
    while (true);
  }

  SPI.begin();
  rfid.PCD_Init();

  if (rfid.PCD_ReadRegister(rfid.VersionReg) == 0x00 || rfid.PCD_ReadRegister(rfid.VersionReg) == 0xFF) {
    Serial.println("RFID not responding");
    digitalWrite(LED_ERROR, HIGH);
    while (true);
  }

  // Wi-Fi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("Connecting Wi-Fi...");
  display.display();

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 15) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi failed.");
    digitalWrite(LED_ERROR, HIGH);
    display.println("WiFi FAILED!");
    display.display();
    while (true);
  }

  digitalWrite(LED_CONNECTED, HIGH); // Wi-Fi connected

  // Welcome
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("System Ready");
  display.setCursor(0, 30);
  display.println("Scan your card...");
  display.display();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uidStr = "0000-0000-0000-0001";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toLowerCase();

  Serial.println("UID: " + uidStr);

  // JSON request
  HTTPClient http;
  http.begin(server);
  http.addHeader("Content-Type", "application/json");

  String json = "{\"stationNumber\":" + String(stationNumber) + ",\"cardId\":\"" + uidStr + "\"}";
  int httpCode = http.POST(json);

  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(2);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Response: " + payload);
    if (payload.indexOf("\"success\":true") > 0) {
      display.println("ACCESS");
      display.println("GRANTED");
      buzz(700);
    } else {
      display.println("ACCESS");
      display.println("DENIED");
      buzz(150);
    }
  } else {
    Serial.println("HTTP error");
    digitalWrite(LED_ERROR, HIGH);
    display.setTextSize(1);
    display.println("Server error");
    buzz(150);
  }

  display.display();
  delay(3000);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) {
    delay(100);
  }

  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Scan your card...");
  display.display();

  digitalWrite(LED_ERROR, LOW); // Reset error LED after each loop
}
