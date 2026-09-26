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
  pinMode(GPIO_RADIO_RESET_N, OUTPUT);
  pinMode(SPI_RADIO_MISO, INPUT);
}

DECLARE_THREAD(radio, RadioState* state) {
  // Read all our data and stuff

}


DECLARE_THREAD(usb_output, RadioState* state) {
  // Write and read to console
  
}

void ARDUINO_ISR_ATTR onRadioInterrupt() {
  digitalWrite(GPIO_LED_ORANGE, HIGH);
  //packet len = 32? could make const
  //could make timeout a const too?
  int tmp = CC1120::recvPacket(packet, 32, timeout); //this int should indiate whether a packet was received
  if (tmp == 1) { //1 or wtv value it returns when it is successfully received

  } 


  digitalWrite(GPIO_LED_ORANGE, LOW);
  
}

void setup() {
  Serial.begin(9600);
  init_gpio();
  Serial.println("Boot");

  digitalWrite(GPIO_RADIO_RESET_N, LOW);
  sleep(0.1);
  digitalWrite(GPIO_RADIO_RESET_N, HIGH);

  // SPI.begin(SPI_RADIO_SCLK, SPI_RADIO_MISO, SPI_RADIO_MOSI);
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

  // radio.sendCommandStrobe(CC112X_CMD_SRES);
  // Serial.println("CC1120 Reset Complete");
  digitalWrite(SPI_RADIO_CS, LOW);
}

void loop() {
  // Serial.print(digitalRead(SPI_RADIO_MISO));
  // if (digitalRead(SPI_RADIO_MISO)) {
  //   digitalWrite(GPIO_LED_ORANGE, HIGH);
  // }
  // else {
  //   digitalWrite(GPIO_LED_ORANGE, LOW);
  // }
  // Nah
  Serial.println(radio.getStatus());
  uint8_t temp;
  radio.readRegister(0x8f, &temp);
  Serial.println(temp);

  if (temp == 0x20) {
    digitalWrite(GPIO_LED_ORANGE, HIGH);
  }
  else {
    digitalWrite(GPIO_LED_ORANGE, LOW);
  }
  
  // for (int i=0; i<255; i++) {
  //   uint8_t temp;
  //   uint8_t status = radio.readRegister(i, &temp);
  //   Serial.println(temp);
  //   if (temp != 255) {
  //     digitalWrite(GPIO_LED_ORANGE, LOW);
  //   }
  //   else {
  //     digitalWrite(GPIO_LED_ORANGE, HIGH);
  //   }
  //   sleep(0.1);
  // }
  // This should never happen
}

