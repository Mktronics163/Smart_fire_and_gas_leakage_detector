#include <Arduino_RouterBridge.h>
int redPin = 2;
void led_on(){
  digitalWrite(2,HIGH);
}
void led_off(){
  digitalWrite(2,LOW);
}
void setup() {
  // put your setup code here, to run once:
Bridge.begin();
pinMode(2,OUTPUT);
Bridge.provide("led_on",led_on);
Bridge.provide("led_off",led_off);

}

void loop() {
  // put your main code here, to run repeatedly:

}
