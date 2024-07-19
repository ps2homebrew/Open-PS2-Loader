/* SYSTEM.CNF file parsing code for PlayStation 2 discs, similar to the system used by OSDSYS to verify the boot filename of a disc.
 * While OSDSYS actually queries the boot filename from the MECHACON and uses this file to verify the filename,
 * this can still be used to get the boot filename without the real disc.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ps2cnf.h"
#include "ioman.h"

#define CNF_LEN_MAX 1024

static const char *CNFGetToken(const char *cnf, const char *end, const char *key)
{
    for (; isspace((int)*cnf) && cnf < end; cnf++) {
    }

    int key_length = strlen(key);
    for (int i = 0; i < key_length; i++, cnf++) {
        if (cnf >= end || *cnf == '\0')
            return (const char *)-1;
        else if (*cnf != key[i])
            return NULL; // Non-match
    }

    return cnf;
}

static const char *CNFAdvanceLine(const char *start, const char *end)
{
    for (; start < end; start++) {
        if (*start == '\n')
            return start + 1 < end ? start + 1 : NULL;
    }

    return NULL;
}

static const char *CNFGetKey(const char *line, char *key)
{
    int i;

    // Skip leading whitespace
    for (; isspace((int)*line); line++) {
    }

    if (*line == '\0') { // Unexpected end of file
        return (const char *)-1;
    }

    for (i = 0; i < CNF_PATH_LEN_MAX && *line != '\0'; i++) {
        if (isgraph((int)*line)) {
            *key = *line;
            line++;
            key++;
        } else if (isspace((int)*line)) {
            *key = '\0';
            break;
        } else if (*line == '\0') { // Unexpected end of file. This check exists, along with the other similar check above.
            return (const char *)-1;
        }
    }

    return line;
}

int ps2cnfGetBootFile(const char *path, char *bootfile)
{
    char system_cnf[CNF_LEN_MAX];
    const char *pChar, *cnf_start, *cnf_end;
    FILE *fd;
    int size;

    if ((fd = fopen(path, "r")) == NULL) {
        LOG("Can't open %s\n", path);
        return ENOENT;
    }

    fseek(fd, 0, SEEK_END);
    size = ftell(fd);
    rewind(fd);

    if (size >= CNF_LEN_MAX)
        size = CNF_LEN_MAX - 1;

    if (fread(system_cnf, 1, size, fd) != size) {
        fclose(fd);
        LOG("Can't read %s\n", path);
        return EIO;
    }
    fclose(fd);

    system_cnf[size] = '\0';
    cnf_end = &system_cnf[size];

    // Parse SYSTEM.CNF
    cnf_start = system_cnf;
    while ((pChar = CNFGetToken(cnf_start, cnf_end, "BOOT2")) == NULL) {
        cnf_start = CNFAdvanceLine(cnf_start, cnf_end);
        if (cnf_start == NULL)
            return -1;
    }

    if (pChar == (const char *)-1) { // Unexpected EOF
        return -1;
    }

    if ((pChar = CNFGetToken(pChar, cnf_end, "=")) == (const char *)-1) { // Unexpected EOF
        return -1;
    }

    if (CNFGetKey(pChar, bootfile) == (const char *)-1) { // Unexpected EOF
        return -1;
    }

    return 0;
}
