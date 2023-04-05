#ifndef INCLUDE_LWNBDSUPPORT_H_
#define INCLUDE_LWNBDSUPPORT_H_

#define LWNBD_SID 0x2A39;

enum LWNBD_SERVER_CMD {
    LWNBD_SERVER_CMD_START,
    LWNBD_SERVER_CMD_STOP,
};

struct lwnbd_config
{
    char defaultexport[32];
    uint8_t readonly;
};

void handleLwnbdSrv();
int NBDInit(void);
void NBDDeinit(void);

#endif /* INCLUDE_LWNBDSUPPORT_H_ */
