#pragma once

#include "ao_packet.h"
#include "cc1120.h"

class AOPacketLink {
public:
    explicit AOPacketLink(CC1120& radio);

    bool start(const char *callsign);
    void stop();
    void loop();

private:
    static constexpr uint8_t encodedPacketLength = 160;
    static constexpr uint8_t decodedPacketLength = sizeof(ao_packet) + 2;
    static constexpr uint32_t packetIntervalMs = 100;

    CC1120& radio;
    uint8_t encoded[encodedPacketLength];
    uint8_t encodedUsed;
    char callsign[AO_MAX_CALLSIGN];
    uint8_t receiveSequence;
    uint8_t transmitSequence;
    bool haveReceiveSequence;
    bool connected;
    bool commandPending;
    bool commandQueued;
    uint32_t nextTransmitAt;
    uint32_t partialPacketAt;

    void receiveBytes();
    void processPacket(const uint8_t *bytes);
    void sendPacket();
    void resetReceive();
};
