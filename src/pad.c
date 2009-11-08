/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include <libpad.h>

int ret;
int port, slot;
int i;
struct padButtonStatus buttons;
u32 paddata;
u32 oldpaddata;
int delaycnt[16];
int paddelay[16];

#define KEY_LEFT 1
#define KEY_DOWN 2
#define KEY_RIGHT 3
#define KEY_UP 4
#define KEY_START 5
#define KEY_R3 6
#define KEY_L3 7
#define KEY_SELECT 8
#define KEY_SQUARE 9
#define KEY_CROSS 10
#define KEY_CIRCLE 11
#define KEY_TRIANGLE 12
#define KEY_R1 13
#define KEY_L1 14
#define KEY_R2 15
#define KEY_L2 16

// KEY_ to PAD_ conversion table
const int keyToPad[17] = {
	-1,
	PAD_LEFT,
	PAD_DOWN,
	PAD_RIGHT,
	PAD_UP,
	PAD_START,
	PAD_R3,
	PAD_L3,
	PAD_SELECT,
	PAD_SQUARE,
	PAD_CROSS,
	PAD_CIRCLE,
	PAD_TRIANGLE,
	PAD_R1,
	PAD_L1,
	PAD_R2,
	PAD_L2
};

/*
 * Global var's
 */
// pad_dma_buf is provided by the user, one buf for each pad
// contains the pad's current state
static char padBuf[256] __attribute__((aligned(64)));

static char actAlign[6];
static int actuators;

/*
 * waitPadReady()
 */
int waitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
            printf("Please wait, pad(%d,%d) is in state %s\n", 
                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        printf("Pad OK!\n");
    }
    return 0;
}


/*
 * initializePad()
 */
int initializePad(int port, int slot){

    int ret;
    int modes;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    printf("The device has %d modes\n", modes);

    if (modes > 0) {
        printf("( ");
        for (i = 0; i < modes; i++) {
            printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
        printf(")");
    }

    printf("It is currently using mode %d\n", 
               padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller 
    // (it has no actuator engines)
    if (modes == 0) {
        printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
        printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        printf("This is no Dual Shock controller??\n");
        return 1;
    }

    printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
    printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);        
    printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        printf("padSetActAlign: %d\n", 
                   padSetActAlign(port, slot, actAlign));
    }
    else {
        printf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}

int ReadPad() {
	int rcode = 0;

        ret = padRead(port, slot, &buttons); // port, slot, buttons
            
        if (ret != 0) {
		u32 newpdata = 0xffff ^ buttons.btns; 
		
		if (newpdata != 0x0) // something
			rcode = 1;
		else
			rcode = 0;

		oldpaddata = paddata;
		paddata = newpdata;
	}

	return rcode;
}

int GetKey(int num) {

	int n, rtn=0;
	
	if ( (num<=0) || (num>=17) )
		return 0;
	
	int keyid = keyToPad[num--];
	
	if(paddata & keyid) {
		delaycnt[num]++;
		if(delaycnt[num]>paddelay[num]) {
			rtn=1;
			
			for(n=0;n<16;n++)
				delaycnt[n]=paddelay[n];
			
			delaycnt[num]=0;
		}
	}

	return rtn;
}

int GetKeyOn(int num) {
	// old v.s. new pad data
	int keyid = keyToPad[num];
	
	return (paddata & keyid) && (!(oldpaddata & keyid));
}

int GetKeyOff(int num) {
	// old v.s. new pad data
	int keyid = keyToPad[num];
	
	return (!(paddata & keyid)) && (oldpaddata & keyid);
}

int GetKeyPressed(int num) {
	// old v.s. new pad data
	int keyid = keyToPad[num];
	
	return (paddata & keyid);
}

void SetButtonDelay(int button, int btndelay){
	paddelay[button-1]=btndelay;
}

void UnloadPad(){
	padPortClose(port, slot);
	padReset();
}

int StartPad(){
	int n=0;
	
    padInit(0);

    port = 0; // 0 -> Connector 1, 1 -> Connector 2
    slot = 0; // Always zero if not using multitap

	padGetPortMax();
	padGetSlotMax(port);
	
    //printf("PortMax: %d\n", padGetPortMax());
    //printf("SlotMax: %d\n", padGetSlotMax(port));
    
    if((ret = padPortOpen(port, slot, padBuf)) == 0) {
        printf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }
    
    if(!initializePad(port, slot)) {
        printf("Error al iniciar el pad\n");
        SleepThread();
    }
	        ret=padGetState(port, slot);
        while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
            if(ret==PAD_STATE_DISCONN) {
                printf("Pad(%d, %d) is disconnected\n", port, slot);
            }
            ret=padGetState(port, slot);
        }
        if(i==1) {
            //printf("Pad: OK!\n");
        }
    
	for(n=0;n<16;n++){
		delaycnt[n]=0;
	}
	
	for(n=0;n<16;n++){
		paddelay[n]=7;
	}
	
    return 0;
}

