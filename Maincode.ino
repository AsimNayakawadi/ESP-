#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ==== OLED CONFIG ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 0  // SDA now on GPIO0
#define OLED_SCL 2  // SCL now on GPIO2
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==== RFID CONFIG ====
#define RST_PIN 22
#define SS_PIN  5
MFRC522 rfid(SS_PIN, RST_PIN);

// ==== LEDS & BUZZER ====
#define LED_SYS_READY 12
#define LED_CONNECTED 14
#define LED_ERROR     27
#define BUZZER_PIN    4

// ==== WIFI CONFIG ====
const char* ssid     = "moto_g53";
const char* password = "19670000";

// ==== SERVER CONFIG ====
const char* server = "http://campusdayapp.germanywestcentral.cloudapp.azure.com/services/station/check";
int stationNumber = 0; // Will be set by DIP switch

// ==== DIP SWITCH GPIOs ====
#define DIP1 32
#define DIP2 33
#define DIP3 25
#define DIP4 26
#define DIP5 13
#define DIP6 15

bool wifiConnected = false;

// ==== HELPERS ====
void buzz(int duration = 700) {
  tone(BUZZER_PIN, 1000, duration);
  
}

int readStationNumber() {
  int value = 0;
  value |= digitalRead(DIP1) == LOW ? 1 << 0 : 0;
  value |= digitalRead(DIP2) == LOW ? 1 << 1 : 0;
  value |= digitalRead(DIP3) == LOW ? 1 << 2 : 0;
  value |= digitalRead(DIP4) == LOW ? 1 << 3 : 0;
  value |= digitalRead(DIP5) == LOW ? 1 << 4 : 0;
  value |= digitalRead(DIP6) == LOW ? 1 << 5 : 0;
  return value;
}

void showMessage(const String& l1,
                 const String& l2 = "",
                 const String& l3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);   // ← guarantees white text
  display.setCursor(0, 10);  display.println(l1);
  if (l2.length()) { display.setCursor(0, 25); display.println(l2); }
  if (l3.length()) { display.setCursor(0, 40); display.println(l3); }
  display.display();
}


void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_SYS_READY, OUTPUT);
  pinMode(LED_CONNECTED, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);

  digitalWrite(LED_SYS_READY, HIGH);
  digitalWrite(LED_CONNECTED, LOW);
  digitalWrite(LED_ERROR, LOW);

  pinMode(DIP1, INPUT_PULLUP);
  pinMode(DIP2, INPUT_PULLUP);
  pinMode(DIP3, INPUT_PULLUP);
  pinMode(DIP4, INPUT_PULLUP);
  pinMode(DIP5, INPUT_PULLUP);
  pinMode(DIP6, INPUT_PULLUP);

  stationNumber = readStationNumber();
  Serial.print("Station Number: ");
  Serial.println(stationNumber);

  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    digitalWrite(LED_ERROR, HIGH);
    while (true);
  }

  showMessage("OLED OK", "Initializing...");

  SPI.begin();
  rfid.PCD_Init();

  if (rfid.PCD_ReadRegister(rfid.VersionReg) == 0x00 || rfid.PCD_ReadRegister(rfid.VersionReg) == 0xFF) {
    Serial.println("RFID not found");
    showMessage("RFID ERROR", "Check wiring!");
    digitalWrite(LED_ERROR, HIGH);
    return;
  }

  WiFi.disconnect(true, true);
  WiFi.begin(ssid, password);
  showMessage("Connecting WiFi...");

  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED || WiFi.localIP().toString() == "0.0.0.0") && retries < 15) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0") {
    Serial.println("WiFi connected");
    digitalWrite(LED_CONNECTED, HIGH);
    wifiConnected = true;
    showMessage("System Ready", "WiFi Connected", "Scan your card...");
  } else {
    Serial.println("WiFi failed");
    showMessage("System Ready", "WiFi Failed", "Offline Mode");
    digitalWrite(LED_ERROR, HIGH);
    wifiConnected = false;
  }
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toLowerCase();

  Serial.println("Scanned UID: " + uidStr);

  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(2);

  if (wifiConnected) {
    HTTPClient http;
    http.begin(server);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"stationNumber\":" + String(stationNumber) + ",\"cardId\":\"" + uidStr + "\"}";
    int httpCode = http.PUT(json);

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
      display.setTextSize(1);
      display.println("Server Error");
      buzz(150);
      digitalWrite(LED_ERROR, HIGH);
    }

    http.end();
  } else {
    display.println("OFFLINE");
    display.println("NO SERVER");
    buzz(150);
  }

  display.display();
  delay(3000);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) {
    delay(100);
  }

  showMessage("Scan your card...");
  digitalWrite(LED_ERROR, LOW);
}
