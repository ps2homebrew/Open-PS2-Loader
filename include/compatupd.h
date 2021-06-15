#ifndef __COMPATUPD_H
#define __COMPATUPD_H

#define OPL_USER_AGENT          "OPL/" OPL_VERSION
#define OPL_COMPAT_HTTP_HOST    "sx.sytes.net"
#define OPL_COMPAT_HTTP_PORT    80
#define OPL_COMPAT_HTTP_RETRIES 3
#define OPL_COMPAT_HTTP_URI     "/oplcl/sync.ashx?code=%s&device=%d"

void oplUpdateGameCompat(int UpdateAll);
int oplGetUpdateGameCompatStatus(unsigned int *done, unsigned int *total);
void oplAbortUpdateGameCompat(void);
int oplUpdateGameCompatSingle(int id, item_list_t *support, config_set_t *configSet);

#endif
