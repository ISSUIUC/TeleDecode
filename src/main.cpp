#include <Arduino.h>
#include <SPI.h>

#include "cc1120_config.h"
#include "pins.h"
#include "hal.h"
#include "radio_state.h"
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

DECLARE_THREAD(radio, RadioState* state) {
  // Read all our data and stuff
}


DECLARE_THREAD(usb_output, RadioState* state) {
  // Write and read to console
  
}

void ARDUINO_ISR_ATTR onRadioInterrupt() {
  digitalWrite(GPIO_LED_ORANGE, HIGH);
  digitalWrite(GPIO_LED_ORANGE, LOW);
}

void setup() {
  Serial.begin(9600);
  init_gpio();
  Serial.println("Boot");
  SPI.begin(SPI_RADIO_SCLK, SPI_RADIO_MISO, SPI_RADIO_MOSI);
  // RadioState state;
  // // attachInterrupt(GPIO_RADIO_INT, onRadioInterrupt, RISING);
  // START_THREAD(radio, SENSOR_CORE, &state, 8);
  // START_THREAD(usb_output, DATA_CORE, &state, 8);
  // while (true) {
  //   THREAD_SLEEP(1000);
  // }

  digitalWrite(GPIO_LED_RED, HIGH);
  digitalWrite(GPIO_LED_BLUE, HIGH);
  digitalWrite(GPIO_LED_GREEN, HIGH);
  digitalWrite(GPIO_LED_ORANGE, HIGH);
  radio.sendCommandStrobe(CC112X_CMD_SRES);
}

void loop() {
  // Nah
  Serial.println(radio.getStatus());
  
  // This should never happen
}

