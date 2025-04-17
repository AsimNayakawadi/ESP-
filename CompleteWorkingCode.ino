#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================================
//  ONLINE MODE – fast scan, auto‑reconnect, red LED when offline
// ============================================================

// ==== OLED CONFIG ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 0           // SDA on GPIO0 (strap pin – keep pull‑up!)
#define OLED_SCL 2           // SCL on GPIO2 (strap pin – keep pull‑up!)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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
const char* ssid     = "moto_g53";
const char* password = "19670000";

// ==== SERVER CONFIG ====
const char* server = "http://campusdayapp.germanywestcentral.cloudapp.azure.com/services/station/check";
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
inline void buzz(uint16_t duration = 200) { tone(BUZZER_PIN, 1000, duration); }

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

void showMessage(const String& l1,const String& l2="",const String& l3=""){
  display.clearDisplay();display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,10);display.println(l1);
  if(l2.length()){display.setCursor(0,25);display.println(l2);} if(l3.length()){display.setCursor(0,40);display.println(l3);} display.display();
}

void bigMessage(const String& a,const String& b){display.clearDisplay();display.setTextSize(2);display.setTextColor(SSD1306_WHITE);display.setCursor(0,8);display.println(a);display.println(b);display.display();}

// -----------------------------------------------------------------------------
//  WIFI MANAGEMENT
// -----------------------------------------------------------------------------
void startWiFi(){Serial.print("Connecting Wi‑Fi … ");WiFi.disconnect(true,true);WiFi.begin(ssid,password);lastReconnectAttempt=millis();}

void serviceWiFi(){
  if(wifiConnected && WiFi.status()==WL_CONNECTED) return;
  if(millis()-lastReconnectAttempt<5000) return; // wait 5 s between tries
  lastReconnectAttempt=millis();
  Serial.print("Wi‑Fi retry … ");WiFi.disconnect();WiFi.begin(ssid,password);
  unsigned long t0=millis();while(millis()-t0<800){if(WiFi.status()==WL_CONNECTED)break;delay(20);}  
  if(WiFi.status()==WL_CONNECTED){Serial.println("connected");wifiConnected=true;digitalWrite(LED_CONNECTED,LOW);digitalWrite(LED_ERROR,LOW);showMessage("Wi‑Fi OK",WiFi.localIP().toString());}
  else {Serial.println("failed");wifiConnected=false;digitalWrite(LED_CONNECTED,LOW);digitalWrite(LED_ERROR,HIGH);}  }

// -----------------------------------------------------------------------------
//  SETUP
// -----------------------------------------------------------------------------
void setup(){Serial.begin(115200);
  pinMode(BUZZER_PIN,OUTPUT);pinMode(LED_SYS_READY,OUTPUT);pinMode(LED_CONNECTED,OUTPUT);pinMode(LED_ERROR,OUTPUT);digitalWrite(LED_SYS_READY,HIGH);
  pinMode(DIP1,INPUT_PULLUP);pinMode(DIP2,INPUT_PULLUP);pinMode(DIP3,INPUT_PULLUP);pinMode(DIP4,INPUT_PULLUP);pinMode(DIP5,INPUT_PULLUP);pinMode(DIP6,INPUT_PULLUP);
  stationNumber=readStationNumber();Serial.printf("Station #: %u\n",stationNumber);
  Wire.begin(OLED_SDA,OLED_SCL);delay(80);if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)){digitalWrite(LED_ERROR,HIGH);while(true);}showMessage("OLED OK","Init system…");
  SPI.begin();rfid.PCD_Init();if(rfid.PCD_ReadRegister(rfid.VersionReg)==0x00||rfid.PCD_ReadRegister(rfid.VersionReg)==0xFF){showMessage("RFID ERROR","Check wiring!");digitalWrite(LED_ERROR,HIGH);while(true);}  
  startWiFi(); }

// -----------------------------------------------------------------------------
//  MAIN LOOP
// -----------------------------------------------------------------------------
void loop(){
  serviceWiFi();
  if(!rfid.PICC_IsNewCardPresent()||!rfid.PICC_ReadCardSerial()) return;
  String uid;for(byte i=0;i<rfid.uid.size;i++){if(rfid.uid.uidByte[i]<0x10)uid+="0";uid+=String(rfid.uid.uidByte[i],HEX);}uid.toLowerCase();Serial.println("UID: "+uid);

  if(wifiConnected){HTTPClient http;http.begin(server);http.addHeader("Content-Type","application/json");String json="{\"stationNumber\":"+String(stationNumber)+",\"cardId\":\""+uid+"\"}";int code=http.PUT(json);
    if(code>0){String payload=http.getString();Serial.println("Server: "+payload);if(payload.indexOf("\"success\":true")>0){bigMessage("ACCESS","GRANTED");buzz();digitalWrite(LED_CONNECTED,HIGH);}else{bigMessage("ACCESS","DENIED");buzz(150);digitalWrite(LED_CONNECTED,LOW);} }
    else {bigMessage("SERVER","ERROR");buzz(150);digitalWrite(LED_CONNECTED,LOW);wifiConnected=false;digitalWrite(LED_CONNECTED,LOW);digitalWrite(LED_ERROR,HIGH);} http.end();}
  else {bigMessage("OFFLINE","GRANTED");buzz();digitalWrite(LED_CONNECTED,LOW);}

  delay(600);
  rfid.PICC_HaltA();rfid.PCD_StopCrypto1();while(rfid.PICC_IsNewCardPresent()||rfid.PICC_ReadCardSerial()){delay(20);}showMessage("Scan your card…"); }
