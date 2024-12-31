/*
 * Manage cheat codes
 *
 * Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
 * Copyright (C) 2014 doctorxyz
 *
 * This file is part of PS2rd, the PS2 remote debugger.
 *
 * PS2rd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2rd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#include "include/opl.h"
#include "include/cheatman.h"
#include "include/ioman.h"
#include <ctype.h>

static int gEnableCheat; // Enables PS2RD Cheat Engine - 0 for Off, 1 for On
static int gCheatMode;   // Cheat Mode - 0 Enable all cheats, 1 Cheats selected by user

static u32 gCheatList[MAX_CHEATLIST]; // Store hooks/codes addr+val pairs
cheat_entry_t gCheats[MAX_CODES];

void InitCheatsConfig(config_set_t *configSet)
{
    config_set_t *configGame = configGetByType(CONFIG_GAME);

    // Default values.
    gCheatSource = 0;
    gEnableCheat = 0;
    gCheatMode = 0;

    if (configGetInt(configSet, CONFIG_ITEM_CHEATSSOURCE, &gCheatSource)) {
        // Load the rest of the per-game CHEAT configuration if CHEAT is enabled.
        if (configGetInt(configSet, CONFIG_ITEM_ENABLECHEAT, &gEnableCheat) && gEnableCheat) {
            configGetInt(configSet, CONFIG_ITEM_CHEATMODE, &gCheatMode);
        }
    } else {
        if (configGetInt(configGame, CONFIG_ITEM_ENABLECHEAT, &gEnableCheat) && gEnableCheat) {
            configGetInt(configGame, CONFIG_ITEM_CHEATMODE, &gCheatMode);
        }
    }
}

int GetCheatsEnabled(void)
{
    return gEnableCheat;
}

const u32 *GetCheatsList(void)
{
    return gCheatList;
}

/*
 * make_code - Return a code object from string @s.
 */
static code_t make_code(const char *s)
{
    code_t code;
    u32 address;
    u32 value;
    char digits[CODE_DIGITS];
    int i = 0;

    while (*s) {
        if (isxdigit((int)*s))
            digits[i++] = *s;
        s++;
    }

    digits[i] = '\0';

    sscanf(digits, "%08X %08X", &address, &value);

    // Return Code Address and Value
    code.addr = address;
    code.val = value;
    return code;
}


/*
 * is_cheat_code - Return non-zero if @s indicates a cheat code.
 *
 * Example: 10B8DAFA 00003F00
 */
static int is_cheat_code(const char *s)
{
    int i = 0;

    while (*s) {
        if (isxdigit((int)*s)) {
            if (++i > CODE_DIGITS)
                return 0;
        } else if (!isspace((int)*s)) {
            return 0;
        }
        s++;
    }

    return (i == CODE_DIGITS);
}

/*
 * parse_line - Parse the current line and return a filled (code found) or zeroed (code not found) object.
 */
static code_t parse_line(const char *line, int linenumber)
{
    code_t code;
    code.addr = 0;
    code.val = 0;
    int ret;
    LOG("%4i  %s\n", linenumber, line);
    ret = is_cheat_code(line);
    if (ret) {
        /* Process actual code and add it to the list. */
        code = make_code(line);
    }
    return code;
}

/*
 * is_cmt_str - Return non-zero if @s indicates a comment.
 */
static inline int is_cmt_str(const char *s)
{
    return (strlen(s) >= 2 && !strncmp(s, "//", 2)) || (*s == '#');
}

/*
 * chr_idx - Returns the index within @s of the first occurrence of the
 * specified char @c.  If no such char occurs in @s, then (-1) is returned.
 */
static size_t chr_idx(const char *s, char c)
{
    size_t i = 0;

    while (s[i] && (s[i] != c))
        i++;

    return (s[i] == c) ? i : -1;
}

/*
 * term_str - Terminate string @s where the callback functions returns non-zero.
 */
static char *term_str(char *s, int (*callback)(const char *))
{
    if (callback != NULL) {
        while (*s) {
            if (callback(s)) {
                *s = NUL;
                break;
            }
            s++;
        }
    }

    return s;
}

/*
 * is_empty_str - Returns 1 if @s contains no printable chars other than white
 * space.  Otherwise, 0 is returned.
 */
static int is_empty_str(const char *s)
{
    size_t slen = strlen(s);

    while (slen--) {
        if (isgraph((int)*s++))
            return 0;
    }

    return 1;
}

/*
 * trim_str - Removes white space from both ends of the string @s.
 */
static int trim_str(char *s)
{
    size_t first = 0;
    size_t last;
    size_t slen;
    char *t = s;

    /* Return if string is empty */
    if (is_empty_str(s))
        return -1;

    /* Get first non-space char */
    while (isspace((int)*t++))
        first++;

    /* Get last non-space char */
    last = strlen(s) - 1;
    t = &s[last];
    while (isspace((int)*t--))
        last--;

    /* Kill leading/trailing spaces */
    slen = last - first + 1;
    memmove(s, s + first, slen);
    s[slen] = NUL;

    return slen;
}

/*
 * is_empty_substr - Returns 1 if the first @count chars of @s are not printable
 * (apart from white space).  Otherwise, 0 is returned.
 */
static int is_empty_substr(const char *s, size_t count)
{
    while (count--) {
        if (isgraph((int)*s++))
            return 0;
    }

    return 1;
}

/* Max line length to parse */
#define CHEAT_LINE_MAX 255

/**
 * parse_buf - Parse a text buffer for cheats.
 * @buf: buffer holding text (must be NUL-terminated!)
 * @return: 0: success, -1: error
 */
static int parse_buf(const char *buf)
{
    code_t code;
    char line[CHEAT_LINE_MAX + 1];
    int linenumber = 1;
    int cheat_index = -1; // Starts at -1.. indicating no cheat being processed yet
    int code_index = 0;
    char temp_name[CHEAT_NAME_MAX + 1] = {0};

    if (buf == NULL)
        return -1;

    while (*buf) {
        /* Scanner */
        int len = chr_idx(buf, LF);
        if (len < 0)
            len = strlen(buf);
        else if (len > CHEAT_LINE_MAX)
            len = CHEAT_LINE_MAX;

        if (!is_empty_substr(buf, len)) {
            strncpy(line, buf, len);
            line[len] = NUL;

            /* Screener */
            term_str(line, is_cmt_str);
            trim_str(line);

            // Check if the line is a cheat name
            if (!is_cheat_code(line) && strlen(line) > 0) {
                // Store the cheat name temporarily
                strncpy(temp_name, line, CHEAT_NAME_MAX);
                temp_name[CHEAT_NAME_MAX] = NUL;
                // Reset code index in case this is a new cheat
                code_index = 0;
            } else {
                /* Parser */
                code = parse_line(line, linenumber);
                if (!(code.addr == 0 && code.val == 0)) {
                    // Only add the cheat entry if we have a valid cheat name in temp_name
                    if (cheat_index < MAX_CODES && temp_name[0] != NUL) {
                        cheat_index++; // Move to the next cheat entry
                        strncpy(gCheats[cheat_index].name, temp_name, CHEAT_NAME_MAX);
                        gCheats[cheat_index].name[CHEAT_NAME_MAX] = NUL;
                        gCheats[cheat_index].enabled = 1; // Set cheat as enabled
                        temp_name[0] = NUL;               // Clear temp_name after use
                    }
                    // Add the cheat code to the current cheat entry
                    if (cheat_index >= 0 && code_index < MAX_CHEATLIST)
                        gCheats[cheat_index].codes[code_index++] = code;
                }
            }
        }
        linenumber++;
        buf += len + 1;
    }

    return 0;
}

/**
 * read_text_file - Reads text from a file into a buffer.
 * @filename: name of text file
 * @maxsize: max file size (0: arbitrary size)
 * @return: ptr to NULL-terminated text buffer, or NULL if an error occured
 */
static inline char *read_text_file(const char *filename, int maxsize)
{
    char *buf = NULL;
    int fd, filesize;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        LOG("%s: Can't open text file %s\n", __FUNCTION__, filename);
        return NULL;
    }

    filesize = lseek(fd, 0, SEEK_END);
    if (filesize < 0) {
        LOG("%s: Can't seek in text file %s\n", __FUNCTION__, filename);
        close(fd);
        return NULL;
    }

    if (maxsize > 0 && filesize > maxsize) {
        LOG("%s: Text file too large: %i bytes, max: %i bytes\n", __FUNCTION__, filesize, maxsize);
        close(fd);
        return NULL;
    }

    buf = malloc(filesize + 1);
    if (buf == NULL) {
        LOG("%s: Unable to allocate %i bytes\n", __FUNCTION__, filesize + 1);
        close(fd);
        return NULL;
    }

    if (filesize > 0) {
        lseek(fd, 0, SEEK_SET);
        if (read(fd, buf, filesize) != filesize) {
            LOG("%s: Can't read from text file %s\n", __FUNCTION__, filename);
            free(buf);
            close(fd);
            return NULL;
        }
    }

    buf[filesize] = '\0';
    close(fd);

    return buf;
}

/*
 * Load cheats from text file.
 */
int load_cheats(const char *cheatfile)
{
    char *buf = NULL;
    int ret;

    memset(gCheats, 0, sizeof(gCheats));

    LOG("%s: Reading cheat file '%s'...\n", __FUNCTION__, cheatfile);
    buf = read_text_file(cheatfile, 0);
    if (buf == NULL) {
        LOG("%s: Could not read cheats file '%s'\n", __FUNCTION__, cheatfile);
        return -1;
    }

    ret = parse_buf(buf);
    free(buf);

    if (ret < 0)
        return ret;

    return (gCheatMode == 0) ? 0 : 1;
}

void set_cheats_list(void)
{
    int cheatCount = 0;

    memset((void *)gCheatList, 0, sizeof(gCheatList));

    // Populate the cheat list
    for (int i = 0; i < MAX_CODES; ++i) {
        if (gCheats[i].enabled) {
            for (int j = 0; j < MAX_CHEATLIST && gCheats[i].codes[j].addr != 0; ++j) {
                if (cheatCount + 2 <= MAX_CHEATLIST) {
                    // Store the address and value in gCheatList
                    gCheatList[cheatCount++] = gCheats[i].codes[j].addr;
                    gCheatList[cheatCount++] = gCheats[i].codes[j].val;
                    LOG("%s: Setting cheat for eecore:\n%s\n%08X %08X\n", __FUNCTION__, gCheats[i].name, gCheats[i].codes[j].addr, gCheats[i].codes[j].val);
                } else
                    break;
            }
        }
    }

    // Append a blank cheat entry if theres space
    if (cheatCount < MAX_CHEATLIST) {
        gCheatList[cheatCount++] = 0; // addr
        gCheatList[cheatCount++] = 0; // val
    } else {
        // Overwrite the last cheat with a blank cheat if we are at max
        gCheatList[cheatCount - 2] = 0; // addr
        gCheatList[cheatCount - 1] = 0; // val
    }
}
