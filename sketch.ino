#include <Arduino.h>
#include "TM1637.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

WiFiUDP Udp;
unsigned int multicastPort = 9522;
IPAddress multicastIP(239,12,255,254);

#define ENCODER_CLK 2
#define ENCODER_DT  3

#define PIR_DT 6

#define DISPLAY_CLK 14
#define DISPLAY_DT 15

volatile int clkLastState;
unsigned int counter = 0;
int displayScreen = 0;
int pirState = LOW;  
int val = 0;

TM1637 tm(DISPLAY_CLK, DISPLAY_DT);

WiFiClientSecure client;
HTTPClient http;
// API for the percentage of renewable energy in germany: https://api.energy-charts.info/ren_share?country=de

void readEncoder() {
  int clkState = digitalRead(ENCODER_CLK);
  int dtState = digitalRead(ENCODER_DT);

  if (clkState != clkLastState) {
    if (dtState != clkState) {
      if(displayScreen >= 2) {
        displayScreen = -1;
      }
      displayScreen++;
      Serial1.println("Up");
    } else {
      if(displayScreen <= 0) {
        displayScreen = 3;
      }
      displayScreen--;
      Serial1.println("Down");
    }
  }
  Serial1.println(displayScreen);

  clkLastState = clkState;

  showDisplay();
}

void checkMotion() {
  val = digitalRead(PIR_DT);
  if (val == HIGH) {
    if (pirState == LOW) {
      Serial1.println("Motion detected!");
      tm.set(7);
      showDisplay();
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH) {
      Serial1.println("Motion ended!");
      tm.clearDisplay();
      tm.set(0);
      pirState = LOW;
    }
  }
}

void showDisplay() {
  Serial1.println("Showing a Display");
  if (pirState != HIGH) {
    return;
  }
  if (displayScreen == 0) {
    currentProd();
  } else if (displayScreen == 1) {
    currentUse();
  } else if (displayScreen == 2) {
    currentPercent();
  }
}

void currentProd() {
  tm.displayStr("Prod");
  tm.displayNum(120);
}

void currentUse() {
tm.displayStr("Use");
tm.displayNum(80);
}

void currentPercent() {
tm.displayStr("Perc");
Serial1.println("Requesting");
client.setInsecure();
http.begin(client, "https://api.energy-charts.info/ren_share?country=de");
http.GET();
Serial1.print(http.getString());
http.end();
}

void listenMulticast() {
  int packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial.printf("Received packet of size %d from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
        
        char* packetBuffer = (char*) malloc(packetSize + 1);
        if (packetBuffer == NULL) {
            Serial.println("Memory allocation failed!");
            return;
        }

        int len = Udp.read(packetBuffer, packetSize);
        packetBuffer[len] = 0;
        for (int i = 56; i < packetSize - 5; i += 8) { // Ensure not to exceed buffer size
          // Assuming little-endian (least significant byte first)
          int b = packetBuffer[i];
          int c = packetBuffer[i + 1];
          int d = packetBuffer[i + 2];
          int e = packetBuffer[i + 3];

          // Check for the OBIS code "1:1.4.0"
          if (e == 1 && d == 4 && c == 1 && b == 1) {
              // Extract the data for "Bezug Wirkleistung" (assuming little-endian)
              int bezug_wirk = ((packetBuffer[i + 5] << 8) | packetBuffer[i + 4]) / 10;
              Serial.printf("Bezug Wirkleistung: %.1f W\n", bezug_wirk / 10.0);
              break; // Exit the loop once you've found the data
          } else {
              Serial.println("Wrong OBIS code: ");
              Serial.print(e, HEX); // Print as hexadecimal
              Serial.print(d, HEX);
              Serial.print(c, HEX);
              Serial.print(b, HEX);
              Serial.println();
          }
        }   

        free(packetBuffer);
    }
}

void setup() {
  Serial1.begin(115200);
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(PIR_DT, INPUT);
  tm.init();
  tm.set(BRIGHT_TYPICAL);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, CHANGE);
  clkLastState = digitalRead(ENCODER_CLK);
  Serial1.println("Started");

  WiFi.begin(ssid, password);
  Serial1.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial1.println("Connected");
  Serial1.print("IP address: ");
  Serial1.println(WiFi.localIP());
  //Udp.beginMulticast(multicastIP, multicastPort);
}

void loop() {
  checkMotion();
  //list Multicast
}