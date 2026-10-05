#include "include/opl.h"
#include "include/announce.h"
#include "include/ethsupport.h"

// PS2RichPresence announce protocol, version 1.
// The wire format is kept in sync with the spec at: https://github.com/brkzlr/PS2RichPresence/blob/master/protocol/ps2rp_protocol.h
#define ANNOUNCE_PORT             50003
#define ANNOUNCE_PROTOCOL_VERSION 1
#define ANNOUNCE_HEADER_SIZE      20
#define ANNOUNCE_TITLE_ID_SIZE    12
#define ANNOUNCE_MAX_NAME_LENGTH  255
#define ANNOUNCE_EVENT_LAUNCH     0
#define ANNOUNCE_SOURCE_OPL       2

void announceGameLaunch(const char *titleId, const char *displayName)
{
    u8 ip[4], netmask[4], gateway[4], broadcast[4];
    u8 packet[ANNOUNCE_HEADER_SIZE + ANNOUNCE_MAX_NAME_LENGTH];
    struct sockaddr_in addr;
    int sock, nameLength, i;

    if (!gEnableAnnounce)
        return;

    if (ethGetNetConfig(ip, netmask, gateway) < 0)
        return;

    memcpy(&packet[0], "PS2R", 4);
    packet[4] = ANNOUNCE_PROTOCOL_VERSION;
    packet[5] = ANNOUNCE_EVENT_LAUNCH;
    packet[6] = ANNOUNCE_SOURCE_OPL;
    nameLength = displayName ? strlen(displayName) : 0;
    if (nameLength > ANNOUNCE_MAX_NAME_LENGTH)
        nameLength = ANNOUNCE_MAX_NAME_LENGTH;
    packet[7] = (u8)nameLength;
    memset(&packet[8], 0, ANNOUNCE_TITLE_ID_SIZE);
    if (titleId)
        strncpy((char *)&packet[8], titleId, ANNOUNCE_TITLE_ID_SIZE - 1);
    memcpy(&packet[ANNOUNCE_HEADER_SIZE], displayName, nameLength);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return;

    for (i = 0; i < 4; i++)
        broadcast[i] = ip[i] | ~netmask[i];

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ANNOUNCE_PORT);
    addr.sin_addr.s_addr = broadcast[0] | (broadcast[1] << 8) | (broadcast[2] << 16) | ((u32)broadcast[3] << 24);

    sendto(sock, packet, ANNOUNCE_HEADER_SIZE + nameLength, 0, (struct sockaddr *)&addr, sizeof(addr));
    disconnect(sock);
}
