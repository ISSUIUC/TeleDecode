/*
 * TeleDecode Board Software
 * Benson Zhou
 * Jennifer Luo
 * Muhammad Ali
 */

#include <Arduino.h>
#include "pins.h"
#include "cc112x_spi.h"

// put function declarations here:

void setup() {
  // put your setup code here, to run once:

  pinMode(GPIO_LED_RED, OUTPUT);
  pinMode(GPIO_LED_ORANGE, OUTPUT);
  pinMode(GPIO_LED_GREEN, OUTPUT);
  pinMode(GPIO_LED_BLUE, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
}
