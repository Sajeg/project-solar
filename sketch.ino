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

// Gets the Data that is to be displayed through a multicast signal send out by this: https://github.com/kettenbach-it/FHEM-SMA-Speedwire/blob/master/77_SMAEM.pm
void listenMulticast() {
  int packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial1.printf("Received packet of size %d from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
        
        char* packetBuffer = (char*) malloc(packetSize + 1);
        if (packetBuffer == NULL) {
            Serial1.println("Memory allocation failed!");
            return;
        }

        int len = Udp.read(packetBuffer, packetSize);
        packetBuffer[len] = 0;
        
        // For Debug purpose
        // for (int i = 0; i < packetSize - 5; i ++) {
        //   int b = packetBuffer[i];
        //   Serial.print("At pos ");
        //   Serial.print(i);
        //   Serial.print(" is value ");
        //   Serial.print(b);
        //   Serial.println();
        // }

        // Only captures every second OBIS Code
        // Couldn't find a way to get every
        // i = 28 or i = 36 depending on what Codes you want to receive
        for (int i = 36; i < packetSize - 5; i += 16) {
          // Check for OBIS Code 1:1.4.0
          if (packetBuffer[i] == 0 && packetBuffer[i+1] == 1 && packetBuffer[i+2] == 8 && packetBuffer[i+3] == 0) {
            float value;
            memcpy(&value, &packetBuffer[i+6], 4);
            // Doesn't work yet
            // To-Do: Look at this https://github.com/datenschuft/SMA-EM
            Serial1.printf("Value (OBIS 1:1.4.0): %.2f W\n", value);
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
