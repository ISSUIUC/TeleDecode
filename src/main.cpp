#include <Arduino.h>
#include <SPI.h>

#include "cc1120_config.h"
#include "pins.h"
#include "hal.h"

CC1120 radio(SPI, SPI_RADIO_CS, GPIO_RADIO_INT);

void init_gpio() {
  pinMode(GPIO_LED_RED, OUTPUT);
  pinMode(GPIO_LED_GREEN, OUTPUT);
  pinMode(GPIO_LED_BLUE, OUTPUT);
  pinMode(GPIO_LED_ORANGE, OUTPUT);
  pinMode(GPIO_RADIO_INT, INPUT);
  pinMode(SPI_RADIO_CS, OUTPUT);
}

DECLARE_THREAD(radio, void* nothing) {
  // Read all our data and stuff  
}


DECLARE_THREAD(usb_output, void* nothing) {
  // Write and read to console
}

void setup() {
  digitalWrite(GPIO_LED_RED, HIGH);
  SPI.begin(SPI_RADIO_SCLK, SPI_RADIO_MISO, SPI_RADIO_MOSI);
  START_THREAD(radio, SENSOR_CORE, NULL, 8);
  START_THREAD(usb_output, DATA_CORE, NULL, 8);
  while (true) {
    THREAD_SLEEP(1000);
  }
}

void loop() {
  // Nah
  // This should never happen
}
