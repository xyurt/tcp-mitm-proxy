#pragma once

#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

int handle_packet_event(SOCKET client, SOCKET server, int from_client, const char *packet, int packet_length) {
    const char *source = from_client ? "Client" : "Server";

    printf("Packet received from %s: length = %d bytes\n", source, packet_length);

    int print_len = packet_length < 32 ? packet_length : 32; 
    printf("Data (hex): ");
    for (int i = 0; i < print_len; i++) {
        printf("%02X ", (unsigned char)packet[i]);
    }
    if (packet_length > 32) {
        printf("... (truncated)");
    }
    printf("\n");

    return 1;
}