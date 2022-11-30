#ifndef intf5220_h
#define intf5220_h

#if defined(__cplusplus) && !defined(USE_CPLUS)
extern "C" {
#endif

struct TMS5220interface
{
   int baseclock;               /* clock rate = 80 * output sample rate,     */
                            /* usually 640000 for 8000 Hz sample rate or */
                            /* usually 800000 for 10000 Hz sample rate.  */
	int volume;
	void (*irq)(void);       /* IRQ callback function */
};

int tms5220_sh_start (struct TMS5220interface *interface);
void tms5220_sh_stop (void);
void tms5220_sh_update (void);

void tms5220_data_w (int offset, int data);
int tms5220_status_r (int offset);
int tms5220_ready_r (void);
int tms5220_int_r (void);

void tms5220_reset (void);

#if defined(__cplusplus) && !defined(USE_CPLUS)
}
#endif
#endif

