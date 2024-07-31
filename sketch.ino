#include <Arduino.h>
#include <TM1637.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <WiFi.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/dns.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* multicastGroup = "239.12.255.254";
const int multicastPort = 9522;

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
}

void checkMotion() {
  val = digitalRead(PIR_DT);
  if (val == HIGH) {
    if (pirState == LOW) {
      Serial1.println("Motion detected!");
      tm.set(7);
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
}

void loop() {
  checkMotion();
  if (pirState == HIGH) {
    tm.display(0, (counter / 1000) % 10);
  tm.display(1, (counter / 100) % 10);
  tm.display(2, (counter / 10) % 10);
  tm.display(3, counter % 10);

  counter++;
  if (counter == 10000) {
    counter = 0;
  }

  }
  delay(100);
}