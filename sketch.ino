#include <Arduino.h>
#include "TM1637.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
int lastClk = HIGH;
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

// Not being used
void readEncoder() {
  int dtValue = digitalRead(ENCODER_DT);
  Serial1.println(dtValue);
  if (dtValue == HIGH) {
    Serial1.println("Rotated clockwise ⏩");
  } else if (dtValue == LOW) {
    Serial1.println("Rotated counterclockwise ⏪");
  }

  showDisplay();

}

void checkMotion() {
  val = digitalRead(PIR_DT);
  if (val == HIGH) {
    if (pirState == LOW) {
      Serial1.println("Motion detected!");
      tm.set(7);
      pirState = HIGH;
      showDisplay();
    }
  } else {
    if (pirState == HIGH) {
      Serial1.println("Motion ended!");
      tm.displayNum(0);
      tm.clearDisplay();
      tm.set(0);
      pirState = LOW;
    }
  }
}

void showDisplay() {
  if (pirState != HIGH) {
    return;
  }
  if (displayScreen == 0) {
    currentUse();
  } else if (displayScreen == 1) {
    currentPercent();
  }
}

void currentUse() {
  tm.displayStr("NET");
  delay(500);
  // Process Multicast data
  // uint8_t packetBuffer[608];
  // int packetSize = Udp.parsePacket();

  // if (packetSize) {
  //   Udp.read(packetBuffer, sizeof(packetBuffer));
  //   EnergyData energyData = parseEnergyMeter(packetBuffer, packetSize);
  //   tm.displayNum(energyData.psupply);
  // }

  // But for this simulated env we use:
  EnergyData energyData = {};
  energyData.pconsume = 2680.0;
  energyData.psupply = 0.0;
  int decimal = 0;

  float displayValue;
  if (energyData.psupply > 0) {
    if (energyData.psupply < 999) {
      displayValue = energyData.psupply;
    } else {
      displayValue = energyData.psupply / 1000;
      decimal = 2;
    }
  } else {
    if (energyData.pconsume < 999) {
      displayValue -= energyData.pconsume;
    } else {
      displayValue -= energyData.pconsume / 1000.0;
      decimal = 2;
    }
  }

  tm.displayNum(displayValue, decimal, true);
}

void currentPercent() {
  tm.displayStr("Perc");
  JsonDocument doc;
  Serial1.println("Requesting");
  client.setInsecure();
  http.begin(client, "https://api.energy-charts.info/ren_share?country=de");
  http.GET();
  deserializeJson(doc, http.getString());
  Serial1.println(doc["data"].as<String>());
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
  // Switched to version in loop because this had a issue, that I couldn't fix
  // attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, CHANGE);
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
  int newClk = digitalRead(ENCODER_CLK);
  if (newClk != lastClk) {
    lastClk = newClk;
    int dtValue = digitalRead(ENCODER_DT);
    if (newClk == LOW && dtValue == HIGH) {
      if(displayScreen >= 1) {
        displayScreen = -1;
      }
      displayScreen ++;
      showDisplay();
    }
    if (newClk == LOW && dtValue == LOW) {
      if(displayScreen <= 0) {
        displayScreen = 2;
      }
      displayScreen --;
      showDisplay();
    }
  }
}