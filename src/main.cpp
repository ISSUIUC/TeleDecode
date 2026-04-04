#include <Arduino.h>
#include "pins.h"
#include "cc1120_config.h"
// #include "cc112x_spi.h"
/*
 * TeleDecode Board Software
 * Benson Zhou
 * Jennifer Luo
 * Muhammad Ali
 */

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  pinMode(SPI_RADIO_CS, OUTPUT);
  digitalWrite(SPI_RADIO_CS, HIGH);

  pinMode(GPIO_LED_RED, OUTPUT);
  pinMode(GPIO_LED_ORANGE, OUTPUT);
  pinMode(GPIO_LED_GREEN, OUTPUT);
  pinMode(GPIO_LED_BLUE, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}
