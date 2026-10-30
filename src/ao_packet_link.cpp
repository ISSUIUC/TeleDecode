#include "ao_packet_link.h"

#include <Arduino.h>
#include <string.h>

#include "ao.h"

static_assert(sizeof(ao_packet) == 76, "Altos packet frame layout changed");

AOPacketLink::AOPacketLink(CC1120& radio)
    : radio(radio),
      encodedUsed(0),
      receiveSequence(0),
      transmitSequence(0),
      haveReceiveSequence(false),
      connected(false),
      commandPending(false),
      commandQueued(false),
      nextTransmitAt(0),
      partialPacketAt(0)
{
    memset(encoded, 0, sizeof(encoded));
    memset(callsign, 0, sizeof(callsign));
}

bool AOPacketLink::start(const char *newCallsign)
{
    size_t callsignLength = strlen(newCallsign);
    if (callsignLength == 0 || callsignLength > AO_MAX_CALLSIGN) {
        return false;
    }

    memset(callsign, 0, sizeof(callsign));
    memcpy(callsign, newCallsign, callsignLength);
    encodedUsed = 0;
    receiveSequence = 0;
    transmitSequence = 0;
    haveReceiveSequence = false;
    connected = false;
    commandPending = false;
    commandQueued = false;
    nextTransmitAt = millis();
    partialPacketAt = 0;

    return radio.configurePacketLink(encodedPacketLength);
}

void AOPacketLink::stop()
{
    encodedUsed = 0;
    connected = false;
    commandPending = false;
    commandQueued = false;
}

void AOPacketLink::loop()
{
    receiveBytes();

    if (static_cast<int32_t>(millis() - nextTransmitAt) < 0) {
        return;
    }

    sendPacket();
    nextTransmitAt = millis() + packetIntervalMs;
}

void AOPacketLink::receiveBytes()
{
    uint8_t chunk[64];
    int bytesRead = radio.readPacketLinkBytes(chunk, sizeof(chunk));
    if (bytesRead < 0) {
        resetReceive();
        return;
    }
    if (bytesRead == 0) {
        if (encodedUsed && millis() - partialPacketAt > packetIntervalMs) {
            resetReceive();
        }
        return;
    }

    if (encodedUsed == 0) {
        partialPacketAt = millis();
    }

    uint8_t remaining = encodedPacketLength - encodedUsed;
    uint8_t copied = min(static_cast<uint8_t>(bytesRead), remaining);
    memcpy(encoded + encodedUsed, chunk, copied);
    encodedUsed += copied;

    if (encodedUsed == encodedPacketLength) {
        processPacket(encoded);
        resetReceive();
    }
}

void AOPacketLink::processPacket(const uint8_t *bytes)
{
    uint8_t decoded[decodedPacketLength];
    if (!ao_fec_decode(bytes, encodedPacketLength, decoded, decodedPacketLength, nullptr) ||
        decoded[decodedPacketLength - 1] != AO_FEC_DECODE_CRC_OK) {
        return;
    }

    const ao_packet *packet = reinterpret_cast<const ao_packet *>(decoded);
    if (memcmp(packet->callsign, callsign, AO_MAX_CALLSIGN) != 0) {
        return;
    }

    connected = true;
    if (commandPending && packet->ack == transmitSequence) {
        commandPending = false;
    }

    if (packet->len == AO_PACKET_SYN) {
        receiveSequence = packet->seq;
        haveReceiveSequence = true;
    } else if (packet->len > 0 && packet->len <= AO_PACKET_MAX) {
        if (!haveReceiveSequence || packet->seq == static_cast<uint8_t>(receiveSequence + 1)) {
            receiveSequence = packet->seq;
            haveReceiveSequence = true;
            Serial.write(packet->d, packet->len);
        }
    }

    if (connected && !commandQueued) {
        commandQueued = true;
        commandPending = true;
        transmitSequence++;
    }
}

void AOPacketLink::sendPacket()
{
    ao_packet packet = {};
    packet.len = AO_PACKET_SYN;
    packet.seq = transmitSequence;
    packet.ack = haveReceiveSequence ? receiveSequence : 0;
    memcpy(packet.callsign, callsign, AO_MAX_CALLSIGN);

    if (commandPending) {
        static const uint8_t monitorCommand[] = {'E', ' ', '0', '\n'};
        packet.len = sizeof(monitorCommand);
        memcpy(packet.d, monitorCommand, sizeof(monitorCommand));
    }

    uint8_t encodedLength = ao_fec_encode(
        reinterpret_cast<const uint8_t *>(&packet),
        sizeof(packet),
        encoded);
    if (encodedLength != encodedPacketLength ||
        !radio.sendPacketLinkBytes(encoded, encodedLength)) {
        Serial.println("Packet Link transmit failed");
    }
}

void AOPacketLink::resetReceive()
{
    encodedUsed = 0;
    partialPacketAt = 0;
}
