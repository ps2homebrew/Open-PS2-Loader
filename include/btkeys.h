#ifndef _BTKEYS_H_
#define _BTKEYS_H_

/*
 * Link keys of Bluetooth pads paired wirelessly (Secure Simple Pairing) from the
 * PADEMU menu. Shared by the OPL GUI (EE), the menu driver (ds34bt.irx) and the
 * in-game driver (bt_pademu.irx).
 *
 * Text form (OPL config value and in-game module argument): one entry per pad,
 * 44 hex characters each = 6 bytes BD_ADDR (HCI byte order) + 16 bytes link key.
 * The in-game module argument prefixes the list with BTKEYS_ARG_TAG.
 */

#define BTKEYS_MAX       4
#define BTKEYS_ENTRY_HEX 44
#define BTKEYS_HEX_LEN   (BTKEYS_MAX * BTKEYS_ENTRY_HEX)
#define BTKEYS_ARG_TAG   "BTK"
#define BTKEYS_ARG_LEN   (3 + BTKEYS_HEX_LEN + 1)

typedef struct
{
    u8 bdaddr[6];
    u8 key[16];
} bt_link_key_t;

static inline int btkeys_hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

// Parses consecutive 44-character entries. Stops at the first incomplete or invalid one.
static inline int btkeys_parse(const char *s, bt_link_key_t *keys, int max)
{
    int n = 0, i, hi, lo;
    u8 raw[22];

    while (n < max) {
        for (i = 0; i < 22; i++) {
            hi = btkeys_hexval(s[2 * i]);
            if (hi < 0)
                return n;
            lo = btkeys_hexval(s[2 * i + 1]);
            if (lo < 0)
                return n;
            raw[i] = (u8)((hi << 4) | lo);
        }
        for (i = 0; i < 6; i++)
            keys[n].bdaddr[i] = raw[i];
        for (i = 0; i < 16; i++)
            keys[n].key[i] = raw[6 + i];
        s += BTKEYS_ENTRY_HEX;
        n++;
    }

    return n;
}

// Writes count entries as hex plus a terminating NUL. out needs count * 44 + 1 bytes.
static inline void btkeys_format(const bt_link_key_t *keys, int count, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    int n, i;
    const u8 *p;

    for (n = 0; n < count; n++) {
        p = keys[n].bdaddr;
        for (i = 0; i < 6; i++) {
            *out++ = hex[p[i] >> 4];
            *out++ = hex[p[i] & 0x0F];
        }
        p = keys[n].key;
        for (i = 0; i < 16; i++) {
            *out++ = hex[p[i] >> 4];
            *out++ = hex[p[i] & 0x0F];
        }
    }
    *out = '\0';
}

static inline int btkeys_same_addr(const u8 *a, const u8 *b)
{
    int i;

    for (i = 0; i < 6; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static inline int btkeys_find(const bt_link_key_t *keys, int count, const u8 *bdaddr)
{
    int n;

    for (n = 0; n < count; n++)
        if (btkeys_same_addr(keys[n].bdaddr, bdaddr))
            return n;
    return -1;
}

// Adds or replaces the key of bdaddr. When the table is full the oldest entry is dropped.
// Returns the new count.
static inline int btkeys_store(bt_link_key_t *keys, int count, const u8 *bdaddr, const u8 *key)
{
    int n, i;

    n = btkeys_find(keys, count, bdaddr);
    if (n < 0) {
        if (count >= BTKEYS_MAX) {
            for (n = 1; n < count; n++) { // byte copy: no memcpy dependency on the IOP
                for (i = 0; i < 6; i++)
                    keys[n - 1].bdaddr[i] = keys[n].bdaddr[i];
                for (i = 0; i < 16; i++)
                    keys[n - 1].key[i] = keys[n].key[i];
            }
            count--;
        }
        n = count++;
        for (i = 0; i < 6; i++)
            keys[n].bdaddr[i] = bdaddr[i];
    }
    for (i = 0; i < 16; i++)
        keys[n].key[i] = key[i];

    return count;
}

#endif
