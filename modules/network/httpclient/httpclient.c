/*  Simple HTTP client for retrieving files.
    Support for the "chunked" encoding has not been implemented.    */

#include <stdio.h>
#include <errno.h>
#include <sysclib.h>
#include <ps2ip.h>

#include "httpclient.h"

void HttpCloseConnection(s32 HttpSocket)
{
    shutdown(HttpSocket, SHUT_RDWR);
    closesocket(HttpSocket);
}

static int EstablishConnection(struct in_addr *server, unsigned short int port)
{
    struct sockaddr_in SockAddr;
    int HostSocket;

    HostSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = server->s_addr;
    SockAddr.sin_port = htons(port);
    if (connect(HostSocket, (struct sockaddr *)&SockAddr, sizeof(SockAddr)) != 0) {
        HttpCloseConnection(HostSocket);
        HostSocket = -1;
    }

    return HostSocket;
}

static int SendData(int socket, char *buffer, int length)
{
    char *pointer;
    int remaining, result;

    for (remaining = length, pointer = buffer; remaining > 0; remaining -= result, pointer += result) {
        if ((result = send(socket, pointer, remaining, 0)) < 1)
            break;
    }

    return length - remaining;
}

static int GetData(int socket, char *buffer, int length)
{
    struct timeval timeout;
    fd_set readfds;
    char *pointer;
    int remaining, ToRead, result;

    for (remaining = length, pointer = buffer; remaining > 0;) {
        ToRead = remaining;

        // This safeguards against a deadlock, if the TCP connection gets broken for long enough. Long enough for the RST packet from the other side gets lost.
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        FD_ZERO(&readfds);
        FD_SET(socket, &readfds);
        if (select(socket + 1, &readfds, NULL, NULL, &timeout) <= 0) {
            break;
        }

        if ((result = recv(socket, pointer, ToRead, 0)) < 1)
            break;
        remaining -= result;
        pointer += result;
        if (result != ToRead) // No further data at the moment.
            break;
    }

    return length - remaining;
}

#define HTTP_WORK_BUFFER_SIZE 256 // Not a really great design, but this must be long enough for the longest line in the HTTP entity.

enum TRANFER_ENCODING {
    TRANFER_ENCODING_PLAIN,
    TRANFER_ENCODING_CHUNKED
};

static int ContentLength;
static short int StatusCode;
static unsigned short int HeaderLineNumber;
// static char TransferEncoding;
static char ConnectionMode;

static void HttpParseEntityLine(const char *line)
{
    char *pColon;

    if ((pColon = strchr(line, ':')) != NULL) {
        for (pColon++; *pColon != '\0'; pColon++) {
            if (look_ctype_table(*pColon) & 2)
                *pColon = tolower(*pColon);
        }
    }

    // printf("%u\t%s\n", HeaderLineNumber, line);

    if (HeaderLineNumber == 0 && strncmp(line, "HTTP/1.1 ", 9) == 0)
        StatusCode = strtoul(line + 9, NULL, 10);

    if (strncmp(line, "Content-Length: ", 16) == 0)
        ContentLength = strtoul(line + 15, NULL, 10);
    /*
    if (strncmp(line, "Transfer-Encoding: ", 19) == 0) {
        ContentLength = -1;
        if (strcmp(line + 19, "chunked") == 0)
            TransferEncoding = TRANFER_ENCODING_CHUNKED;
    }
    */
    if (strcmp(line, "Connection: close") == 0)
        ConnectionMode = HTTP_CMODE_CLOSED;

    HeaderLineNumber++;
}

static int HttpGetResponse(s32 socket, s8 *mode, char *buffer, u16 *length)
{
    char work[HTTP_WORK_BUFFER_SIZE + 1], EndOfEntity, *ptr, *next_ptr, *PayloadPtr;
    int result, DataAvailable, PayloadAmount;

    // TransferEncoding = TRANFER_ENCODING_PLAIN;
    ConnectionMode = *mode;
    ContentLength = -1;
    StatusCode = -1;
    HeaderLineNumber = 0;
    PayloadPtr = buffer;
    PayloadAmount = 0;
    EndOfEntity = 0;
    ptr = work;
    DataAvailable = 0;
    work[HTTP_WORK_BUFFER_SIZE] = '\0';
    do {
        if ((result = GetData(socket, ptr, HTTP_WORK_BUFFER_SIZE - DataAvailable)) > 0) {
            DataAvailable += result;
            ptr = work;
            while ((next_ptr = strstr(ptr, "\r\n")) != NULL) {
                *next_ptr = '\0';
                // Parse line
                HttpParseEntityLine(ptr);
                DataAvailable -= (next_ptr + 2 - ptr);
                ptr = next_ptr + 2; // skip CRLN
                if (strncmp(ptr, "\r\n", 2) == 0) {
                    EndOfEntity = 1;
                    DataAvailable -= 2;
                    ptr += 2;
                    break;
                }
            }

            // At this point, the final line (preceding NULL terminator) has been reached. Move any outstanding characters to the start of the buffer.
            if (!EndOfEntity) {
                if (DataAvailable > 0) {
                    memmove(work, ptr, DataAvailable);
                    work[DataAvailable] = '\0';
                    ptr = &work[DataAvailable];
                } else
                    ptr = work;
            }
        } else {
            // No more data. Connection lost?
            //             printf("DEBUG: connection lost?\n");
            break;
        }
    } while (!EndOfEntity);

    // Receive data normally (plain).
    if (DataAvailable > 0) // Move leftover data into the output buffer.
    {
        memcpy(buffer, ptr, DataAvailable);
        buffer[DataAvailable] = '\0';
        PayloadPtr = buffer + DataAvailable;
        PayloadAmount = DataAvailable;
    }

    if (ContentLength < 0 || PayloadAmount < ContentLength) {
        if (ContentLength > *length)
            ContentLength = *length;
        if ((result = GetData(socket, PayloadPtr, ContentLength - PayloadAmount)) > 0)
            *length = PayloadAmount + result;
        else
            *length = PayloadAmount;
    } else
        *length = PayloadAmount;

    result = StatusCode;
    if (ContentLength > 0 && ContentLength > *length) {
        result = -EPIPE; // Incomplete transfer.
                         // printf("Pipe broken: %d/%d\n", *length, ContentLength);
    }

    *mode = ConnectionMode;

    return result;
}

static int ResolveHostname(char *hostname, struct in_addr *ip)
{
    struct hostent *HostEntry;
    struct in_addr **addr_list;

    if ((HostEntry = gethostbyname(hostname)) == NULL)
        return 1;

    for (addr_list = (struct in_addr **)HostEntry->h_addr_list; addr_list != NULL; addr_list++) {
        ip->s_addr = (*addr_list)->s_addr;
        return 0;
    }

    return 1;
}

int HttpEstabConnection(char *server, u16 port)
{
    struct in_addr ip;
    int result;

    if (ResolveHostname(server, &ip) == 0) {
        result = EstablishConnection(&ip, port);
    } else {
        result = -ENXIO;
    }

    return result;
}

static const char *GetDayInWeek(const unsigned char *mtime)
{
    static const unsigned char daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const char *dayLabels[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    unsigned short int DaysInYear;
    unsigned char LeapDays, month;
    unsigned int days;

    LeapDays = mtime[0] / 4; // Number of leap days, in the years elasped.
    for (month = 0, DaysInYear = 0; month < mtime[1]; DaysInYear += daysInMonth[month], month++)
        ; // Number of days, within the months elasped within the past year.
    if (mtime[0] % 4 == 0) {
        if (mtime[1] > 1)
            DaysInYear++; // Account for this year's leap day, if applicable.
    } else
        LeapDays++; // Account for the leap day, of the leap year that just passed.
    days = mtime[0] * 365 + LeapDays + DaysInYear + mtime[2];

    return dayLabels[(5 + days) % 7]; // 2000/1/1 was a Saturday (5).
}

int HttpSendGetRequest(s32 HttpSocket, const char *UserAgent, const char *host, s8 *mode, const u8 *mtime, const char *uri, char *output, u16 *out_len)
{
    const char *months[] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"};
    char buffer[512];
    int result, length;

    sprintf(buffer, "GET %s HTTP/1.1\r\n"
                    "Accept: text/html, */*\r\n"
                    "User-Agent: %s\r\n"
                    "Host: %s\r\n",
            uri, UserAgent, host);

    if (*mode == HTTP_CMODE_PERSISTENT)
        strcat(buffer, "Proxy-Connection: Keep-Alive\r\n");
    if (mtime != NULL)
        sprintf(&buffer[strlen(buffer)], "If-Modified-Since: %s, %02u %s %04u %02u:%02u:%02u GMT\r\n", GetDayInWeek(mtime), mtime[2] + 1, months[mtime[1]], 2000 + mtime[0], mtime[3], mtime[4], mtime[5]);
    strcat(buffer, "\r\n");

    length = strlen(buffer);

    if (SendData(HttpSocket, buffer, length) == length) {
        result = HttpGetResponse(HttpSocket, mode, output, out_len);
    } else {
        result = -1;
    }

    return result;
}
