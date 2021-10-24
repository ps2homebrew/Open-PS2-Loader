/*  Basic NetBIOS Name Service resolver client.
    This client does not completely implement RFC1002,
    as it only allows network addresses to be resolved.

    Related reading materials: RFC1001 and RFC1002.    */

#include <errno.h>
#include <intrman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <ps2ip.h>

#include "nbns.h"

#define MAX_DATAGRAM_LENGTH 576

#define BSWAP16(x) (((x) << 8 & 0xFF00) | ((x) >> 8 & 0xFF))

#define NB_OPCODE(R, OPCODE)           (((R) << 4) | (OPCODE))
#define NB_MN_FLAGS(AA, TC, RD, RA, B) (((AA) << 6) | ((TC) << 5) | ((RD) << 4) | ((RA) << 3) | (B))
#define NB_CODE(OPCODE, FLAGS, RCODE)  (((OPCODE) << 11) | ((FLAGS) << 4) | (RCODE))

#define NB_GET_RCODE(CODE)    ((CODE)&0xF)
#define NB_GET_MN_FLAGS(CODE) (((CODE) >> 4) & 0x7F)
#define NB_GET_OPCODE(CODE)   (((CODE) >> 11) & 0xF)
#define NB_GET_R(CODE)        (((CODE) >> 15) & 1)

enum NB_OPCODE_TYPE {
    NB_OPCODE_TYPE_QUERY = 0,
    NB_OPCODE_TYPE_REG,
    NB_OPCODE_TYPE_REL,
    NB_OPCODE_TYPE_WACK,
    NB_OPCODE_TYPE_REFRESH,

    NB_OPCODE_TYPE_COUNT
};

#define QUESTION_TYPE_NB  0x0020 // NetBIOS general Name Service Resource Record
#define QUESTION_CLASS_IN 0x0001 // Internet class

struct NbHeader
{
    u16 TransactionID;
    u16 code;
    u16 QDCount;
    u16 ANCount;
    u16 NSCount;
    u16 ARCount;
};

struct NbQuestionTrailer
{
    u16 qnType;
    u16 qnClass;
};

// Record status bits.
#define NBNS_NAME_RECORD_ALLOCATED 0x01
#define NBNS_NAME_RECORD_INIT      0x02
#define NBNS_NAME_RECORD_VALID     0x04

struct NBNSNameRecord
{
    unsigned char status;
    char name[17];
    unsigned char address[4];
    unsigned short int uid;
    int thid;
};

#define MAX_NAME_RECORDS 1

static int nbnsSocket = -1;
static int nbnsReceiveThreadID = -1;
static struct NBNSNameRecord NameRecords[MAX_NAME_RECORDS];

static unsigned char encode_name(const char *name, char *output)
{
    int i;
    unsigned char checksum;
    const char *p;
    char *pOut;

    for (checksum = 0, p = name, pOut = output + 1, i = 0; i < output[0]; i += 2, pOut += 2) {
        if (*p != '\0') {
            pOut[0] = ((*p & 0xF0) >> 4) + 'A';
            pOut[1] = (*p & 0x0F) + 'A';
            p++;
        } else {
            pOut[0] = ((' ' & 0xF0) >> 4) + 'A';
            pOut[1] = (' ' & 0x0F) + 'A';
        }
        checksum += pOut[0] + pOut[1];
    }
    *pOut = '\0';

    return checksum;
}

static unsigned char decode_name(const char *name, char *output)
{
    int i;
    unsigned char len;
    const char *p;
    char *pOut;

    for (p = name + 1, pOut = output, i = 0, len = 0; i < name[0]; i += 2, pOut++, p += 2) {
        if ((*pOut = ((p[0] - 'A') << 4) | ((p[1] - 'A') & 0xF)) != ' ')
            len++;
    }
    *pOut = '\0';

    return len;
}

static struct NBNSNameRecord *AllocNameRecord(void)
{
    int i, OldState;
    struct NBNSNameRecord *result;

    result = NULL;
    CpuSuspendIntr(&OldState);

    for (i = 0; i < MAX_NAME_RECORDS; i++) {
        if (!(NameRecords[i].status & NBNS_NAME_RECORD_ALLOCATED)) {
            NameRecords[i].status = NBNS_NAME_RECORD_ALLOCATED;
            result = &NameRecords[i];
            break;
        }
    }

    CpuResumeIntr(OldState);

    return result;
}

static void FreeNameRecord(struct NBNSNameRecord *record)
{
    int OldState;

    CpuSuspendIntr(&OldState);
    record->status = 0;
    CpuResumeIntr(OldState);
}

static struct NBNSNameRecord *lookupName(const char *name, unsigned short int uid)
{
    int i, OldState;
    struct NBNSNameRecord *result;

    result = NULL;

    CpuSuspendIntr(&OldState);

    for (i = 0; i < MAX_NAME_RECORDS; i++) {
        if ((NameRecords[i].status & NBNS_NAME_RECORD_ALLOCATED) && (strcmp(NameRecords[i].name, name) == 0) && (uid == NameRecords[i].uid)) {
            result = &NameRecords[i];
            break;
        }
    }

    CpuResumeIntr(OldState);

    return result;
}

static void nbnsReceiveThread(void *arg)
{
    static char frame[MAX_DATAGRAM_LENGTH];
    struct NBNSNameRecord *record;
    char decoded_name[17];
    struct NbHeader *header;
    int length, OldState, result;
    unsigned short int code;

    while (1) {
        if ((length = recv(nbnsSocket, frame, sizeof(frame), 0)) > 0) {
            if (length >= sizeof(struct NbHeader)) {
                header = (struct NbHeader *)frame;
                code = BSWAP16(header->code);

                //    printf("R: %d, opcode: 0x%x, rcode: 0x%x, tcode: 0x%04x, flags: 0x%x\n", NB_GET_R(code), NB_GET_OPCODE(code), NB_GET_RCODE(code), BSWAP16(header->TransactionID), NB_GET_MN_FLAGS(code));

                switch (NB_GET_OPCODE(code)) {
                    case NB_OPCODE_TYPE_QUERY:
                        if (NB_GET_R(code)) {
                            // Responses.
                            if (NB_GET_RCODE(code) == 0 && BSWAP16(header->ANCount) == 1) // Positive query response.
                            {
                                result = decode_name(&frame[sizeof(struct NbHeader)], decoded_name);
                                decoded_name[result] = '\0';

                                if ((record = lookupName(decoded_name, BSWAP16(header->TransactionID))) != NULL && (record->status & NBNS_NAME_RECORD_INIT)) {
                                    if (!(record->status & NBNS_NAME_RECORD_VALID)) {
                                        // Set the record as authoritative.
                                        CpuSuspendIntr(&OldState);
                                        memcpy(record->address, &frame[sizeof(struct NbHeader) + 34 + sizeof(struct NbQuestionTrailer) + 4 + 2 + 2], sizeof(record->address));
                                        record->status |= NBNS_NAME_RECORD_VALID;
                                        CpuResumeIntr(OldState);

                                        //        printf("Received IP %u.%u.%u.%u for %s\n", record->address[0], record->address[1], record->address[2], record->address[3], record->name);

                                        if (record->thid >= 0)
                                            WakeupThread(record->thid);
                                    } else {
                                        // Conflict. Don't handle.
                                    }
                                }
                            }
                        }

                        break;
                    case NB_OPCODE_TYPE_WACK:
                    case NB_OPCODE_TYPE_REL:
                    case NB_OPCODE_TYPE_REG:
                    case NB_OPCODE_TYPE_REFRESH:
                        // Do nothing for unsupported codes.
                        break;
                    default:
                        printf("nbnsReceiveThread: unrecognized opcode: %x\n", NB_GET_OPCODE(code));
                }
            }
        } else {
            printf("nbnsReceiveThread: recv returned %d\n", length);
            break;
        }
    }
}

int nbnsInit(void)
{
    int result;
    struct sockaddr_in service;
    iop_thread_t thread;

    if ((nbnsSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0) {
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = htonl(INADDR_ANY);
        service.sin_port = htons(0);

        if (bind(nbnsSocket, (struct sockaddr *)&service, sizeof(service)) >= 0) {
            thread.thread = &nbnsReceiveThread;
            thread.stacksize = 0x350;
            thread.priority = 0x21;
            thread.attr = TH_C;
            thread.option = 0x0B0B0000;

            if ((nbnsReceiveThreadID = CreateThread(&thread)) > 0) {
                StartThread(nbnsReceiveThreadID, NULL);
                result = 0;
            } else {
                nbnsDeinit();
                result = -1;
            }
        } else {
            result = -1;
        }

        if (result < 0)
            lwip_close(nbnsSocket);
    } else {
        result = -1;
    }

    return result;
}

void nbnsDeinit(void)
{
    iop_thread_info_t ThreadInfo;

    if (nbnsSocket >= 0) {
        shutdown(nbnsSocket, SHUT_RDWR);
        closesocket(nbnsSocket);
        nbnsSocket = -1;
    }

    if (nbnsReceiveThreadID > 0) {
        // Wait for the receive thread to terminate.
        while (ReferThreadStatus(nbnsReceiveThreadID, &ThreadInfo) == 0 && ThreadInfo.status != THS_DORMANT)
            DelayThread(1000);
        DeleteThread(nbnsReceiveThreadID);
        nbnsReceiveThreadID = -1;
    }

    memset(NameRecords, 0, sizeof(NameRecords));
}

static unsigned int nbnsQueryNameTimeout(void *arg)
{
    iWakeupThread(((struct NBNSNameRecord *)arg)->thid);
    return 0;
}

int nbnsFindName(const char *name, unsigned char *ip_address)
{
    static unsigned char uid = 0;
    struct NBNSNameRecord *record;
    struct NbHeader *header;
    struct NbQuestionTrailer *trailer;
    char frame[128], question_name[32];
    int result, TotalLength, OldState, retries;
    unsigned char checksum;
    iop_sys_clock_t clock;

    printf("nbnsFindName: %s\n", name);

    if ((record = AllocNameRecord()) != NULL) {
        strcpy(record->name, name);
        CpuSuspendIntr(&OldState);
        record->status |= NBNS_NAME_RECORD_INIT;
        CpuResumeIntr(OldState);
        record->thid = GetThreadId();
        frame[sizeof(struct NbHeader)] = 32;
        checksum = encode_name(name, &frame[sizeof(struct NbHeader)]);
        frame[sizeof(struct NbHeader) + 33] = 0x00;

        header = (struct NbHeader *)frame;
        trailer = (struct NbQuestionTrailer *)(frame + sizeof(struct NbHeader) + sizeof(question_name) + 2);
        record->uid = checksum | ((unsigned short int)uid << 8);
        header->TransactionID = BSWAP16(record->uid);
        uid++;
        header->code = BSWAP16(NB_CODE(NB_OPCODE(0, NB_OPCODE_TYPE_QUERY), NB_MN_FLAGS(0, 0, 1, 0, 1), 0));
        header->QDCount = BSWAP16(1);
        header->ANCount = 0;
        header->NSCount = 0;
        header->ARCount = 0;
        trailer->qnType = BSWAP16(QUESTION_TYPE_NB);
        trailer->qnClass = BSWAP16(QUESTION_CLASS_IN);

        TotalLength = sizeof(struct NbHeader) + 34 + sizeof(struct NbQuestionTrailer);
        // Like with Microsoft Windows, make up to 10 attempts.
        for (retries = 10; retries > 0; retries--) {
            struct sockaddr_in service;

            service.sin_family = AF_INET;
            service.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            service.sin_port = htons(137);

            if (sendto(nbnsSocket, frame, TotalLength, 0, (struct sockaddr *)&service, sizeof(service)) == TotalLength) {
                clock.lo = 27600000; // 250ms * 3 = 750ms of 36.8MHz clock ticks
                clock.hi = 0;
                SetAlarm(&clock, &nbnsQueryNameTimeout, record);
                SleepThread();
                CancelAlarm(&nbnsQueryNameTimeout, record);

                if (record->status & NBNS_NAME_RECORD_VALID) {
                    memcpy(ip_address, record->address, sizeof(record->address));
                    CpuSuspendIntr(&OldState);
                    record->status &= ~NBNS_NAME_RECORD_INIT;
                    CpuResumeIntr(OldState);
                    result = 0;
                    break;
                } else {
                    result = -ENXIO;
                }
            } else {
                result = -1;
            }
        }

        FreeNameRecord(record);
    } else {
        result = -ENOMEM;
    }

    return result;
}
