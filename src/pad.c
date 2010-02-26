/*
  Copyright 2009, Ifcaro
  Licenced under Academic Free License version 3.0
  Review OpenUsbLd README & LICENSE files for further details.  
*/

#include "include/usbld.h"
#include "include/pad.h"
#include <libpad.h>


#define MAX_PADS 4
#define DEFAULT_PAD_DELAY 7

struct pad_data_t {
	int port, slot;
	u32 paddata;
	u32 oldpaddata;
	struct padButtonStatus buttons;

	// pad_dma_buf is provided by the user, one buf for each pad
	// contains the pad's current state
	char padBuf[256] __attribute__((aligned(64)));

	char actAlign[6];
	int actuators;
};

unsigned short pad_count;
struct pad_data_t pad_data[MAX_PADS];

// gathered pad data
u32 paddata;
u32 oldpaddata;

int delaycnt[16];
int paddelay[16];


int ret;
int i;

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
 * waitPadReady()
 */
int waitPadReady(struct pad_data_t* pad) {
	int state;
	
	// busy wait for the pad to get ready
	do {
		state = padGetState(pad->port, pad->slot);
	} while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) && (state != PAD_STATE_DISCONN));
    	
	return state;
};

int initializePad(struct pad_data_t* pad) {
	int ret;
	int modes;
	int i;
	
	// is there any device connected to that port?
	if (waitPadReady(pad) == PAD_STATE_DISCONN) {
		return 1; // nope, don't waste your time here!
	}
	
	// How many different modes can this device operate in?
	// i.e. get # entrys in the modetable
	modes = padInfoMode(pad->port, pad->slot, PAD_MODETABLE, -1);
	printf("The device has %d modes\n", modes);

	if (modes > 0) {
		printf("( ");
		
		for (i = 0; i < modes; i++) {
			printf("%d ", padInfoMode(pad->port, pad->slot, PAD_MODETABLE, i));
		}
	        
	        printf(")");
	}

	printf("It is currently using mode %d\n", 
	padInfoMode(pad->port, pad->slot, PAD_MODECURID, 0));

	// If modes == 0, this is not a Dual shock controller 
	// (it has no actuator engines)
	if (modes == 0) {
		printf("This is a digital controller?\n");
		return 1;
	}

	// Verify that the controller has a DUAL SHOCK mode
	i = 0;
	do {
		if (padInfoMode(pad->port, pad->slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
			break;
		i++;
	} while (i < modes);
	
	if (i >= modes) {
		printf("This is no Dual Shock controller\n");
		return 1;
	}

	// If ExId != 0x0 => This controller has actuator engines
	// This check should always pass if the Dual Shock test above passed
	ret = padInfoMode(pad->port, pad->slot, PAD_MODECUREXID, 0);
	if (ret == 0) {
	        printf("This is no Dual Shock controller??\n");
	        return 1;
	}

	printf("Enabling dual shock functions\n");

	// When using MMODE_LOCK, user cant change mode with Select button
	padSetMainMode(pad->port, pad->slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

	waitPadReady(pad);
	printf("infoPressMode: %d\n", padInfoPressMode(pad->port, pad->slot));

	waitPadReady(pad);        
	printf("enterPressMode: %d\n", padEnterPressMode(pad->port, pad->slot));

	waitPadReady(pad);
	pad->actuators = padInfoAct(pad->port, pad->slot, -1, 0);
	printf("# of actuators: %d\n", pad->actuators);

	if (pad->actuators != 0) {
		pad->actAlign[0] = 0;   // Enable small engine
		pad->actAlign[1] = 1;   // Enable big engine
		pad->actAlign[2] = 0xff;
		pad->actAlign[3] = 0xff;
		pad->actAlign[4] = 0xff;
		pad->actAlign[5] = 0xff;

		waitPadReady(pad);
		printf("padSetActAlign: %d\n", 
		padSetActAlign(pad->port, pad->slot, pad->actAlign));
	} else {
		printf("Did not find any actuators.\n");
	}

	waitPadReady(pad);

	return 1;
}

int readPad(struct pad_data_t* pad)
{
	int rcode = 0;

	ret = padRead(pad->port, pad->slot, &pad->buttons); // port, slot, buttons
            
        if (ret != 0)
	{
		u32 newpdata = 0xffff ^ pad->buttons.btns; 
		
		if (newpdata != 0x0) // something
			rcode = 1;
		else
			rcode = 0;

		pad->oldpaddata = pad->paddata;
		pad->paddata = newpdata;
		
		// merge into the global vars
		paddata |= pad->paddata;
	}

	return rcode;
}

/** polling method. Call every frame. */
int readPads() {
	int i;
	oldpaddata = paddata;
	paddata = 0;
	
	int rslt = 0;
	
	for (i = 0; i < pad_count; ++i) {
		rslt |= readPad(&pad_data[i]);
	}
	
	return rslt;
}

/** Key getter with key repeats.
* @param id The button ID
* @return nonzero if button is being pressed just now
*/
int getKey(int id) {
	int rtn=0;
	
	if ( (id<=0) || (id>=17) )
		return 0;

	// either the button was not pressed this frame, then reset counter and return
	// or it wasn't, then handle the repetition
	if (getKeyOn(id)) {
		delaycnt[id] = 0;
		return 1;
	}	
	
	if(!getKeyPressed(id)) {
		delaycnt[id - 1] = 0;
		return 0;
	}

	id--;
	
	// it is pressed - process the repetition
	delaycnt[id]++;
	
	if(delaycnt[id] > paddelay[id]) {
		rtn=1;
		delaycnt[id]=0;
	}
	

	return rtn;
}

/** Detects key-on event. Returns true if the button was not pressed the last frame but is pressed this frame.
* @param id The button ID
* @return nonzero if button is being pressed just now
*/
int getKeyOn(int id) {
	if ( (id<=0) || (id>=17) )
		return 0;
	
	// old v.s. new pad data
	int keyid = keyToPad[id];
	
	return (paddata & keyid) && (!(oldpaddata & keyid));
}

/** Detects key-off event. Returns true if the button was pressed the last frame but is not pressed this frame.
* @param id The button ID
* @return nonzero if button is being released
*/
int getKeyOff(int id) {
	if ( (id<=0) || (id>=17) )
		return 0;
	
	// old v.s. new pad data
	int keyid = keyToPad[id];
	
	return (!(paddata & keyid)) && (oldpaddata & keyid);
}

/** Returns true (nonzero) if the button is currently pressed
* @param id The button ID
* @return nonzero if button is being held
*/
int getKeyPressed(int id) {
	// old v.s. new pad data
	int keyid = keyToPad[id];
	
	return (paddata & keyid);
}

/** Sets the delay to wait for button repetition event to occur.
* @param button The button ID
* @param btndelay The button delay (in query count)
*/
void setButtonDelay(int button, int btndelay) {
	paddelay[button-1] = btndelay;
}

/** Unloads a single pad. 
* @see unloadPads */
void unloadPad(struct pad_data_t* pad) {
	padPortClose(pad->port, pad->slot);
}

/** Unloads all pads. Use to terminate the usage of the pads. */
void unloadPads() {
	int i;
	
	for (i = 0; i < pad_count; ++i)
		unloadPad(&pad_data[i]);
	
	padReset();
}

/** Tries to start a single pad.
* @param pad The pad data holding structure 
* @return 0 Error, != 0 Ok */
int startPad(struct pad_data_t* pad) {
	if(padPortOpen(pad->port, pad->slot, pad->padBuf) == 0) {
		return 0;
	}
	
	if(!initializePad(pad)) {
	        return 0;
	}

	waitPadReady(pad);
	return 1;
}

/** Starts all pads. 
* @return Count of dual shock compatible pads. 0 if none present. */
int startPads() {
	// scan for pads that exist... at least one has to be present
	pad_count = 0;

	int maxports = padGetPortMax();

	int port; // 0 -> Connector 1, 1 -> Connector 2
	int slot; // Always zero if not using multitap
		
	for (port = 0; port < maxports; ++port) {
		int maxslots = padGetSlotMax(port);
		
		for (slot = 0; slot < maxslots; ++slot) {
		
			struct pad_data_t* cpad = &pad_data[pad_count];
			
			cpad->port = port;
			cpad->slot = slot;
			
			if (startPad(cpad))
				++pad_count;
		}
		
		if (pad_count >= MAX_PADS)
			break; // enough already!
	}
	
	int n;
	for(n=0; n<16; ++n) {
		delaycnt[n]=0;
		paddelay[n]=DEFAULT_PAD_DELAY;
	}
	
	return pad_count;
}

