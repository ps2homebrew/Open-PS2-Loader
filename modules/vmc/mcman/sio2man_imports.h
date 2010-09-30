#ifndef __SIO2MAN_IMPORTS_H__
#define __SIO2MAN_IMPORTS_H__

#include "types.h"
#include "irx.h"

#define SIO2MAN 	0
#define XSIO2MAN 	1

struct _sio2_dma_arg { // size 12
	void	*addr;
	int	size;
	int	count; 
};

typedef struct {
	u32	stat6c;
	u32	port_ctrl1[4];
	u32	port_ctrl2[4]; 
	u32	stat70;
	u32	regdata[16]; 
	u32	stat74; 
	u32	in_size; 
	u32	out_size; 
	u8	*in;
	u8	*out; 
	struct _sio2_dma_arg in_dma; 
	struct _sio2_dma_arg out_dma;
} sio2_transfer_data_t;


// sio2man exports
/* 24 */ void (*sio2_mc_transfer_init)(void);
/* 25 */ int  (*sio2_transfer)(sio2_transfer_data_t *sio2data);

// xsio2man exports
/* 26 */ void (*sio2_transfer_reset)(void);
/* 55 */ int  (*sio2_func1)(void *arg);

#endif
