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

// Only the total data and without cosphi and frequency as I don't need them
struct EnergyData {
    float pconsume;
    float psupply;
    float qconsume;
    float qsupply;
    float sconsume;
    float ssupply;
};

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

// Reworked approache, based on this: https://github.com/datenschuft/SMA-EM/
EnergyData parseEnergyMeter(uint8_t* packetBuffer, int packetSize) {
    EnergyData data = {};

    // 28 is the Header, so if it's smaller than the Header it can't be right
    if (packetSize < 28 || memcmp(packetBuffer, "SMA", 3) != 0) {
        return data;
    }

    int position = 28;
    while (position < packetSize) {
        uint16_t measurement = (packetBuffer[position] << 8) | packetBuffer[position + 1];
        uint8_t datatype = packetBuffer[position + 2];

        if (datatype == 4) {
            int32_t value = (packetBuffer[position + 4] << 24) | (packetBuffer[position + 5] << 16) |
                            (packetBuffer[position + 6] << 8) | packetBuffer[position + 7];
            float scaledValue = value / 10.0f;

            switch (measurement) {
                case 1: data.pconsume = scaledValue; break;
                case 2: data.psupply = scaledValue; break;
                case 3: data.qconsume = scaledValue; break;
                case 4: data.qsupply = scaledValue; break;
                case 9: data.sconsume = scaledValue; break;
                case 10: data.ssupply = scaledValue; break;
            }
            position += 8;
        } else if (datatype == 8) {
            position += 12;
        } else {
            position += 8;
        }
    }

    return data;
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
  // Look for explanation on why this is commented out below
  // Udp.beginMulticast(multicastIP, multicastPort);
}

void loop() {
  checkMotion();


  // Doesn't work in this sumalted enviroment. because it requires to be in my local network
  // I tested it on a esp32 that I had lying around
  uint8_t packetBuffer[608];
  int packetSize = Udp.parsePacket();

  if (packetSize) {
    Udp.read(packetBuffer, sizeof(packetBuffer));

    EnergyData energyData = parseEnergyMeter(packetBuffer, packetSize);

    Serial1.printf("pconsume: %.2f W\n", energyData.pconsume);
    Serial1.printf("psupply: %.2f W\n", energyData.psupply);
    Serial1.printf("qconsume: %.2f W\n", energyData.qconsume);
    Serial1.printf("qsupply: %.2f W\n", energyData.qsupply);
    Serial1.printf("sconsume: %.2f W\n", energyData.sconsume);
    Serial1.printf("ssupply: %.2f W\n", energyData.ssupply);
    }
}
