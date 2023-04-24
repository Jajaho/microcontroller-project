#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_DotStar.h>

#include "main.h"

int laststate = 0;

Adafruit_DotStar strip(1, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SWITCH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH), button_pressed, CHANGE);

  strip.begin();
  strip.setBrightness(80);
  strip.setPixelColor(0, random(0, 255), random(0, 255), random(0, 255));
  strip.show();

  Serial.begin(115200);
  while(!Serial) delay(10);
  Serial.println("Setup");
}

void button_pressed(){
  int state = !digitalRead(SWITCH);
  if(state != laststate){
    laststate = state;
    Serial.printf("Switch: %i\n", state);
    if(state){
      strip.setPixelColor(0, random(0, 255), random(0, 255), random(0, 255));
      strip.show();
    }
  }
}

void loop() {
  //digitalWrite(LED_BUILTIN,HIGH);
  //delay(500);
  //digitalWrite(LED_BUILTIN,LOW);
  //delay(250);

  for(int fadeValue = 0; fadeValue <= 255; fadeValue++){
    analogWrite(LED_BUILTIN, fadeValue);
    delay(1);
  }

  for(int fadeValue = 255; fadeValue >= 0; fadeValue--){
    analogWrite(LED_BUILTIN, fadeValue);
    delay(1);
  }


}