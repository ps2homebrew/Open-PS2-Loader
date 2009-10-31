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
int delaycnt[16];
int paddelay[16];

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

void ReadPad(){
        ret = padRead(port, slot, &buttons); // port, slot, buttons
            
        if (ret != 0) {
            paddata = 0xffff ^ buttons.btns;
		}
}

int GetKey(int num){

	int n, rtn=0;
	
	switch(num){
		case KEY_LEFT:
			if(paddata & PAD_LEFT){
				delaycnt[0]++;
				if(delaycnt[0]>paddelay[0]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[0]=0;
				}
			}
			break;
		case KEY_DOWN:
			if(paddata & PAD_DOWN){
				delaycnt[1]++;
				if(delaycnt[1]>paddelay[1]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[1]=0;
				}
			}
			break;
		case KEY_RIGHT:
			if(paddata & PAD_RIGHT){
				delaycnt[2]++;
				if(delaycnt[2]>paddelay[2]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[2]=0;
				}
			}
			break;
		case KEY_UP:
			if(paddata & PAD_UP){
				delaycnt[3]++;
				if(delaycnt[3]>paddelay[3]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[3]=0;
				}
			}
			break;
		case KEY_START:
			if(paddata & PAD_START){
				delaycnt[4]++;
				if(delaycnt[4]>paddelay[4]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[4]=0;
				}
			}
			break;
		case KEY_R3:
			if(paddata & PAD_R3){
				delaycnt[5]++;
				if(delaycnt[5]>paddelay[5]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[5]=0;
				}
			}
			break;
		case KEY_L3:
			if(paddata & PAD_L3){
				delaycnt[6]++;
				if(delaycnt[6]>paddelay[6]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[6]=0;
				}
			}
			break;
		case KEY_SELECT:
			if(paddata & PAD_SELECT){
				delaycnt[7]++;
				if(delaycnt[7]>paddelay[7]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[7]=0;
				}
			}
			break;
		case KEY_SQUARE:
			if(paddata & PAD_SQUARE){
				delaycnt[8]++;
				if(delaycnt[8]>paddelay[8]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[8]=0;
				}
			}
			break;
		case KEY_CROSS:
			if(paddata & PAD_CROSS){
				delaycnt[9]++;
				if(delaycnt[9]>paddelay[9]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[9]=0;
				}
			}
			break;
		case KEY_CIRCLE:
			if(paddata & PAD_CIRCLE){
				delaycnt[10]++;
				if(delaycnt[10]>paddelay[10]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[10]=0;
				}
			}
			break;
		case KEY_TRIANGLE:
			if(paddata & PAD_TRIANGLE){
				delaycnt[11]++;
				if(delaycnt[11]>paddelay[11]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[11]=0;
				}
			}
			break;
		case KEY_R1:
			if(paddata & PAD_R1){
				delaycnt[12]++;
				if(delaycnt[12]>paddelay[12]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[12]=0;
				}
			}
			break;
		case KEY_L1:
			if(paddata & PAD_L1){
				delaycnt[13]++;
				if(delaycnt[13]>paddelay[13]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[14]=0;
				}
			}
			break;
		case KEY_R2:
			if(paddata & PAD_R2){
				delaycnt[14]++;
				if(delaycnt[14]>paddelay[14]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[14]=0;
				}
			}
			break;
		case KEY_L2:
			if(paddata & PAD_L2){
				delaycnt[15]++;
				if(delaycnt[15]>paddelay[15]){
					rtn=1;
					for(n=0;n<16;n++)delaycnt[n]=paddelay[n];
					delaycnt[15]=0;
				}
			}
			break;
	}

	return rtn;
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

