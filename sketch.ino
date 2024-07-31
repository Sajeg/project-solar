#include <Arduino.h>
#include <TM1637.h>

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
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH) {
      Serial1.println("Motion ended!");
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
}

void loop() {
  checkMotion();
  tm.display(0, (counter / 1000) % 10);
  tm.display(1, (counter / 100) % 10);
  tm.display(2, (counter / 10) % 10);
  tm.display(3, counter % 10);

  counter++;
  if (counter == 10000) {
    counter = 0;
  }

  delay(100);
}