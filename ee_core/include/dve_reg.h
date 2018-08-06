/*
#
# DVE management.
#-------------------------------------------------------------
# Copyright (C) Mega Man, 2010-2014
#
# Taken from Kernelloader.
#
*/

#ifndef _DVE_REG_H_
#define _DVE_REG_H_

void dve_prepare_bus(void);
int dve_get_reg(u32 reg);
int dve_set_reg(u32 reg, u32 val);

#endif
