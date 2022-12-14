/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"



unsigned char *vastar_sharedram;



void vastar_init_machine(void)
{
	cpu_halt(1,0);
}

void vastar_hold_cpu2_w(int offset,int data)
{
	/* I'm not sure that this works exactly like this */
	if ((data & 1) == 0)
	{
		cpu_halt(1,0);
	}
	else
	{
		cpu_halt(1,1);
		cpu_reset(1);
	}
}



int vastar_sharedram_r(int offset)
{
	return vastar_sharedram[offset];
}

void vastar_sharedram_w(int offset, int data)
{
	vastar_sharedram[offset] = data;
}
