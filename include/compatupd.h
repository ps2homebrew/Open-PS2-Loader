#ifndef __COMPATUPD_H
#define __COMPATUPD_H

#define OPL_USER_AGENT "OPL/" OPL_VERSION
#define OPL_COMPAT_HTTP_HOST "sx.sytes.net"
#define OPL_COMPAT_HTTP_PORT 80
#define OPL_COMPAT_HTTP_RETRIES 3
#if OPL_IS_DEV_BUILD
#define OPL_COMPAT_HTTP_URI "/oplcl/sync.ashx?code=%s&device=%d&dev=1"
#else
#define OPL_COMPAT_HTTP_URI "/oplcl/sync.ashx?code=%s&device=%d"
#endif

void oplUpdateGameCompat(int UpdateAll);
int oplGetUpdateGameCompatStatus(unsigned int *done, unsigned int *total);
void oplAbortUpdateGameCompat(void);
int oplUpdateGameCompatSingle(int id, item_list_t *support, config_set_t *configSet);

#endif
