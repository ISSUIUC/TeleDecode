#pragma once

#include "Queue.h"
#include "ao_packet.h"

struct RadioState {
    Queue<ao_packet_recv> rx_queue;
    Queue<ao_packet> tx_queue;
};
