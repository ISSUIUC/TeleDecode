#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <string.h>

#include "cc1120_config.h"
#include "pins.h"
#include "hal.h"
#include "radio_state.h"
#include "cc1120.h"
#include "ao_telemetry.h"
#include "ao_packet_link.h"

CC1120 radio(SPI, SPI_RADIO_CS, SPI_RADIO_MISO, GPIO_RADIO_INT);
AOPacketLink packetLink(radio);
Preferences preferences;

char receiverCallsign[AO_MAX_CALLSIGN + 1] = "N0CALL";
char serialCommand[32];
uint8_t serialCommandLength = 0;
bool serialCommandOverflow = false;
bool preferencesReady = false;
bool packetLinkMode = false;

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

bool orangeLed = false;
bool packetReady = false;

bool validCallsign(const char *callsign) {
  size_t length = strlen(callsign);
  if (length == 0 || length > AO_MAX_CALLSIGN) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    if (callsign[i] < '!' || callsign[i] > '~') {
      return false;
    }
  }
  return true;
}

void startPacketLink() {
  packetReady = false;
  packetLinkMode = false;
  if (!packetLink.start(receiverCallsign)) {
    radio.restorePacketLink();
    Serial.println("Unable to start Packet Link");
    return;
  }
  packetLinkMode = true;
  Serial.printf("Packet Link active; callsign %s\n", receiverCallsign);
}

void stopPacketLink() {
  if (!packetLinkMode) {
    return;
  }
  packetLink.stop();
  radio.restorePacketLink();
  packetLinkMode = false;
  packetReady = false;
  Serial.println("Pad telemetry mode restored");
}

void handleSerialCommand(char *command) {
  if (strcmp(command, "AUTO") == 0) {
    startPacketLink();
  } else if (strcmp(command, "PAD") == 0) {
    stopPacketLink();
  } else if (strncmp(command, "CALL ", 5) == 0) {
    const char *newCallsign = command + 5;
    if (!validCallsign(newCallsign)) {
      Serial.println("Callsign must be 1-8 printable non-space characters");
      return;
    }
    if (preferencesReady &&
        preferences.putString("callsign", newCallsign) == 0) {
      Serial.println("Failed to save callsign");
      return;
    }
    memcpy(receiverCallsign, newCallsign, strlen(newCallsign) + 1);
    Serial.printf("Callsign set to %s\n", receiverCallsign);
    if (packetLinkMode) {
      packetLink.stop();
      startPacketLink();
    }
  } else if (strcmp(command, "HELP") == 0) {
    Serial.println("Commands: CALL <callsign>, AUTO, PAD, HELP");
  } else if (command[0] != '\0') {
    Serial.println("Unknown command; send HELP");
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    char value = static_cast<char>(Serial.read());
    if (value == '\r') {
      continue;
    }
    if (value == '\n') {
      if (serialCommandOverflow) {
        Serial.println("Command too long");
      } else {
        serialCommand[serialCommandLength] = '\0';
        handleSerialCommand(serialCommand);
      }
      serialCommandLength = 0;
      serialCommandOverflow = false;
      continue;
    }
    if (serialCommandLength < sizeof(serialCommand) - 1) {
      serialCommand[serialCommandLength++] = value;
    } else {
      serialCommandOverflow = true;
    }
  }
}


DECLARE_THREAD(radio, RadioState* state) {
  // Read all our data and stuff

}


DECLARE_THREAD(usb_output, RadioState* state) {
  // Write and read to console
  
}

void ARDUINO_ISR_ATTR onRadioInterrupt() {
  digitalWrite(GPIO_LED_ORANGE, orangeLed ? HIGH: LOW);
  orangeLed = !orangeLed;
  packetReady = true;
  // packet len = 32? could make const
  // could make timeout a const too?
}

void setup() {
  Serial.begin(9600);
  preferencesReady = preferences.begin("teledecode", false);
  if (preferencesReady) {
    String savedCallsign = preferences.getString("callsign", "N0CALL");
    if (validCallsign(savedCallsign.c_str())) {
      memcpy(receiverCallsign, savedCallsign.c_str(), savedCallsign.length() + 1);
    } else {
      Serial.println("Stored callsign is invalid; using N0CALL");
    }
  } else {
    Serial.println("Callsign storage unavailable; using N0CALL");
  }
  init_gpio();
  Serial.println("Boot");

  digitalWrite(GPIO_RADIO_RESET_N, LOW);
  delay(100);
  digitalWrite(GPIO_RADIO_RESET_N, HIGH);
  delay(100);

  

  SPI.begin(SPI_RADIO_SCLK, SPI_RADIO_MISO, SPI_RADIO_MOSI);
  //RadioState state;
  attachInterrupt(GPIO_RADIO_INT, onRadioInterrupt, FALLING);
  // START_THREAD(radio, SENSOR_CORE, &state, 8);
  // START_THREAD(usb_output, DATA_CORE, &state, 8);
  // while (true) {
  //   THREAD_SLEEP(1000);
  // }
  

  digitalWrite(GPIO_LED_RED, HIGH);
  digitalWrite(GPIO_LED_BLUE, HIGH);
  digitalWrite(GPIO_LED_ORANGE, HIGH);
  digitalWrite(GPIO_LED_GREEN, HIGH);

  radio.applyConfiguration(cc1200_settings, 55);
  radio.applyConfiguration(packet_setup_384, 5);
  radio.applyConfiguration(packet_setup, sizeof(packet_setup) / sizeof(packet_setup[0]));

  radio.setFrequency(435.450);              // 434.550 MHz @ 40 MHz XOSC * changed freq
  radio.sendCommandStrobe(CC1200_SCAL);      // recalibrate synth for the new freq
  delay(2);
  radio.sendCommandStrobe(CC1200_SRX);
  radio.writeRegister(CC1200_PKT_LEN, 32);
}

void loop() {
  readSerialCommands();

  if (packetLinkMode) {
    packetLink.loop();
    return;
  }

  // Serial.print(digitalRead(SPI_RADIO_MISO));
  // if (digitalRead(SPI_RADIO_MISO)) {
  //   digitalWrite(GPIO_LED_ORANGE, HIGH);
  // }
  // else {
  //   digitalWrite(GPIO_LED_ORANGE, LOW);
  // }
  // Nah
  // Serial.println(radio.getStatus());
  // Serial.println(cc1120_settings[40].addr); //doesnt print after 41 for some reason
  uint8_t temp;
  rfStatus_t status = radio.readRegister(0x2f8f, &temp);
  // Serial.println(temp);
  // Serial.println(status);
  uint8_t test;
  radio.readRegister(CC1200_PA_CFG1, &test); //works!
  // Serial.println("loop");
  // Serial.println(test);

  // if (temp == 0x20) {
  //   digitalWrite(GPIO_LED_ORANGE, HIGH);
  // }
  // else {
  //   digitalWrite(GPIO_LED_ORANGE, LOW);
  // }
  
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
  if (packetReady) {
    uint8_t data[AO_MAX_TELEMETRY];
    // Serial.println("data received");

    //(uint8_t*) packet 
    int tmp = radio.recvPacket(data, 32, 50); //this int should indiate whether a packet was received
    if (tmp == 1) { //1 or wtv value it returns when it is successfully received
      // digitalWrite(GPIO_LED_BLUE, HIGH); 
      // // delay(2000);
      digitalWrite(GPIO_LED_BLUE, LOW);
      //then print the packet to console
      Serial.println("packet rx success");
      for (uint8_t b : data) {
        Serial.printf("%02x ", b);
      }
      Serial.println();
      ao_telemetry_generic * telem = (ao_telemetry_generic *) data;
      Serial.printf("serial: %u\n", telem->serial);
      Serial.printf("tick: %u\n", telem->tick);
      Serial.printf("type: %u\n", telem->type);
      Serial.println("-------------------");
    } 
    else {
      digitalWrite(GPIO_LED_BLUE, HIGH);
    }
    radio.sendCommandStrobe(CC1200_SRX);
    packetReady = false;
  }
  // digitalWrite(GPIO_LED_ORANGE, LOW);
}
