#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define OLED_SDA 0
#define OLED_SCL 2


// ============================================================
//  ONLINE MODE – fast scan, auto‑reconnect, red LED when offline
// ============================================================
// no need to #define SDA/SCL or SW_I2C any more
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R2,             // 180° rotation
  /* reset=*/ U8X8_PIN_NONE
);

// ==== RFID CONFIG ====
#define RST_PIN 22
#define SS_PIN  5
MFRC522 rfid(SS_PIN, RST_PIN);

// ==== LEDS & BUZZER ====
#define LED_SYS_READY 12
#define LED_CONNECTED 14
#define LED_ERROR     27  // solid RED whenever Wi‑Fi / server offline
#define BUZZER_PIN     4

// ==== WIFI CONFIG ====
const char* ssid     = "G2-ESP";
const char* password = "cFdwpYxQocnzeD48";

// ==== SERVER CONFIG ====
const char* server = "http://141.18.1.124:8080/api/station/check";
int stationNumber = 0; // read from DIP switches

// ==== DIP SWITCH GPIOs ====
#define DIP1 32
#define DIP2 33
#define DIP3 25
#define DIP4 26
#define DIP5 13
#define DIP6 15

bool wifiConnected = false;
unsigned long lastReconnectAttempt = 0;   // non‑blocking retry timer

// -----------------------------------------------------------------------------
//  HELPERS
// -----------------------------------------------------------------------------
inline void buzz(uint16_t duration = 200)
{
  tone(BUZZER_PIN, 1000, duration);
}

inline uint8_t readStationNumber() {
  uint8_t v = 0;
  v |= digitalRead(DIP1) == LOW ? 1 << 0 : 0;
  v |= digitalRead(DIP2) == LOW ? 1 << 1 : 0;
  v |= digitalRead(DIP3) == LOW ? 1 << 2 : 0;
  v |= digitalRead(DIP4) == LOW ? 1 << 3 : 0;
  v |= digitalRead(DIP5) == LOW ? 1 << 4 : 0;
  v |= digitalRead(DIP6) == LOW ? 1 << 5 : 0;
  return v;
}

// ───── U8g2-based text helpers ─────
void showMessage(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, l1);
  if (l2) u8g2.drawStr(0, 26, l2);
  if (l3) u8g2.drawStr(0, 40, l3);
  u8g2.sendBuffer();
}

void bigMessage(const char* a, const char* b) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);  // pick any large font
  u8g2.drawStr(0, 30, a);
  u8g2.drawStr(0, 60, b);
  u8g2.sendBuffer();
}
// ────────────────────────────────────

// ────────────────────────────────────────────────────
// -----------------------------------------------------------------------------
//  WIFI MANAGEMENT
// -----------------------------------------------------------------------------
void serviceWiFi(){
  Serial.print("Wi‑Fi connecting ...");
  WiFi.mode(WIFI_STA);
  //unsigned long t0=millis();
  WiFi.begin(ssid,password);
  for(int i = 0; i <= 5000 && WiFi.status() != WL_CONNECTED ; ){
        Serial.print(".");
        delay(100);
        i += 100;
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("connected");
    wifiConnected=true;
    digitalWrite(LED_CONNECTED,LOW);
    digitalWrite(LED_ERROR,LOW);
    showMessage("Wi‑Fi OK",WiFi.localIP().toString().c_str());
  }
  else {
    Serial.println("attempt failed");
    wifiConnected=false;
    digitalWrite(LED_CONNECTED,LOW);
    digitalWrite(LED_ERROR,HIGH);
  }
}

// -----------------------------------------------------------------------------
//  SETUP
// -----------------------------------------------------------------------------
void setup(){
  Serial.begin(115200);
  pinMode(BUZZER_PIN,OUTPUT);
  Wire.begin(OLED_SDA, OLED_SCL);
  pinMode(LED_SYS_READY,OUTPUT);
  pinMode(LED_CONNECTED,OUTPUT);
  pinMode(LED_ERROR,OUTPUT);
  digitalWrite(LED_SYS_READY,HIGH);
  pinMode(DIP1,INPUT_PULLUP);
  pinMode(DIP2,INPUT_PULLUP);
  pinMode(DIP3,INPUT_PULLUP);
  pinMode(DIP4,INPUT_PULLUP);
  pinMode(DIP5,INPUT_PULLUP);
  pinMode(DIP6,INPUT_PULLUP);

  stationNumber=readStationNumber();
  Serial.printf("Station #: %u\n",stationNumber);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "OLED OK");
  u8g2.sendBuffer();
  SPI.begin();
  rfid.PCD_Init();
  if(rfid.PCD_ReadRegister(rfid.VersionReg)==0x00||rfid.PCD_ReadRegister(rfid.VersionReg)==0xFF){
    showMessage("RFID ERROR","Check wiring!");
    digitalWrite(LED_ERROR,HIGH);
    //while(true); test
  }  
  
  if(WiFi.status()!=WL_CONNECTED && (millis()-lastReconnectAttempt>=5000)){
    lastReconnectAttempt=millis();
    serviceWiFi(); 
  }
}

// -----------------------------------------------------------------------------
//  MAIN LOOP
// -----------------------------------------------------------------------------
void loop(){
  if(WiFi.status()!=WL_CONNECTED && (millis()-lastReconnectAttempt>=5000)){
    lastReconnectAttempt=millis();
    serviceWiFi(); 
  }

  if(!rfid.PICC_IsNewCardPresent()||!rfid.PICC_ReadCardSerial()) return;
  String uid;for(byte i=0;i<rfid.uid.size;i++){if(rfid.uid.uidByte[i]<0x10)uid+="0";uid+=String(rfid.uid.uidByte[i],HEX);}uid.toLowerCase();Serial.println("UID: "+uid);

  if(wifiConnected){
    HTTPClient http;
    http.begin(server);
    http.addHeader("Content-Type","application/json");
    String json="{\"stationNumber\":"+String(stationNumber)+",\"cardId\":\""+uid+"\"}";
    int code=http.PUT(json);
    Serial.print("Sending " + String(json));
    if(code>0){
      String payload=http.getString();
      Serial.println("Server: "+payload);
      if(payload.indexOf("\"success\":true")>0){
        bigMessage("ACCESS","GRANTED");
        buzz();
        digitalWrite(LED_CONNECTED,HIGH);
      }
      else{
        bigMessage("ACCESS","DENIED");
        buzz(150);
        digitalWrite(LED_CONNECTED,LOW);
      }
    }
    else{
      Serial.print("Error: " + String(code));
      bigMessage("SERVER","ERROR");
      buzz(150);
      digitalWrite(LED_CONNECTED,LOW);
      wifiConnected=false;
      digitalWrite(LED_CONNECTED,LOW);
      digitalWrite(LED_ERROR,HIGH);
    }
    http.end();
  }
  else{
    bigMessage("OFFLINE","GRANTED");
    buzz();
    digitalWrite(LED_CONNECTED,LOW);
  }

  delay(600);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  while(rfid.PICC_IsNewCardPresent()||rfid.PICC_ReadCardSerial()){
    delay(20);
  }
  showMessage("Scan your card...");
}