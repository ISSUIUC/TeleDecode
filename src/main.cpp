#include <Arduino.h>
#include <SPI.h>
#include "cc1120_config.h"
#include "pins.h"
#include "cc1120.h"

CC1120 radio(SPI, SPI_RADIO_CS, SPI_RADIO_MISO, GPIO_RADIO_INT);

void init_gpio() {
  pinMode(GPIO_LED_RED, OUTPUT);
  pinMode(GPIO_LED_GREEN, OUTPUT);
  pinMode(GPIO_LED_BLUE, OUTPUT);
  pinMode(GPIO_LED_ORANGE, OUTPUT);
  pinMode(GPIO_RADIO_INT, INPUT);
  pinMode(SPI_RADIO_CS, OUTPUT);
}

void ARDUINO_ISR_ATTR onRadioInterrupt() {
  digitalWrite(GPIO_LED_ORANGE, HIGH);
}

void setup() {
  digitalWrite(GPIO_LED_RED, HIGH);
  SPI.begin(SPI_RADIO_SCLK, SPI_RADIO_MISO, SPI_RADIO_MOSI);
  attachInterrupt(GPIO_RADIO_INT, onRadioInterrupt, RISING);
}

void loop() {
  // put your main code here, to run repeatedly:
}
