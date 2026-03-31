#include <Arduino.h>
#include "cc112x_spi.h"
#include "pins.h"
#include "cc1120_config.h"

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
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}