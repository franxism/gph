#include <linux/input.h>
#include "wiz_lib.h"
#include "te9_tf9_hybrid_driver.h"
#include "usbjoy_mame.h"
#include "malloc.h"

#define FB0_0 (0x5600000)
#define FB0_1 (0x5600000+320*240*2)
#define FB1_0 (0x5600000+320*240*4)
#define FB1_1 (0x5600000+320*240*6)
#define FBX_L (320*240*2)
unsigned char *uppermem;
unsigned char *upper_fb;

/* register access */
static unsigned long wiz_dev[3];
static volatile unsigned int *memregs32;
static volatile unsigned short *memregs16;
static volatile unsigned char *memregs8;
static unsigned int bkregs32[15];	/* backing up values */
static unsigned int sec_bkregs32[15];	/* backing up values */

/* library variables */
static int layer_width[2];

unsigned char *fb0_8bit, *fb1_8bit; /* current buffers (8 bit) */
unsigned short *fb0_16bit, *fb1_16bit; /* current buffers (16 bit) */
static unsigned short *fb0_0, *fb0_1; /* layer 0, buffer 0 : layer 0, buffer 1 (RGB) */
static unsigned short *fb1_0, *fb1_1; /* layer 1, buffer 0 : layer 1, buffer 1 (RGB) */

int	wiz_sound_rate=22050;
int	wiz_sound_stereo=0;
int wiz_clock=533;
int rotate_controls=0;
int	wiz_ram_tweaks=0;
int wiz_rotated_video=0;
int gsensor=0;

static void lc_setfb(int layer, unsigned short *set_to);
static void lc_flipfb(int layer,int single);
static void lc_setlayer(int layer, bool onoff, bool alpha, bool invert, bool trans, unsigned int mode);
static void lc_layerpos(int layer, int x1, int y1, int x2, int y2);
/*
static void lc_setalpha(int layer, int value);
static void lc_settranscol(int layer, unsigned int colour);
static void lc_setinvcol(int layer, unsigned int colour);
*/
static void lc_dirtymlc(void);
static void lc_dirtylayer(int layer);
static void lc_screensize(int w, int h);
static void lc_setbgcol(unsigned int colour);
static void lc_setstride(int layer, int hs, int vs);

static void lc_setfb(int layer, unsigned short *set_to)
{
	/* set absolute address for framebuffers */
	if(layer == 0) {
		if(set_to == fb0_0) {
			MLCADDRESS0 = MLC2ADDRESS0 = FB0_0;
		} else {
			MLCADDRESS0 = MLC2ADDRESS0 = FB0_1;
		}
	} else {
		if(set_to == fb1_0) {
			MLCADDRESS1 = MLC2ADDRESS1 = FB1_0;
		} else {
			MLCADDRESS1 = MLC2ADDRESS1 = FB1_1;
		}
	}
	lc_dirtylayer(layer);
}

/* Flips to the other buffer for a particular layer */
static void lc_flipfb(int layer,int single)
{
	/* switch to the other buffer */
	static int current_0 = 0;
	static int current_1 = 0;

    /* single buffer */
    if (single)
    {
    	if(layer == 0) {
    	    current_0=0;
    		lc_setfb(0, fb0_0);
		    fb0_16bit = fb0_0;
		    fb0_8bit = (unsigned char *)fb0_16bit;
            
    	} else {
    	    current_1=0;
    		lc_setfb(1, fb1_0);
		    fb1_16bit = fb1_0;
		    fb1_8bit = (unsigned char *)fb1_16bit;
    	}
    }
    /* double buffer */
    else
    {
    	if(layer == 0) {
    		current_0 = !current_0;
    		lc_setfb(0, current_0 ? fb0_1 : fb0_0);
		    fb0_16bit = current_0 ? fb0_0 : fb0_1;
		    fb0_8bit = (unsigned char *)fb0_16bit;
    	} else {
    		current_1 = !current_1;
    		lc_setfb(1, current_1 ? fb1_1 : fb1_0);
		    fb1_16bit = current_1 ? fb1_0 : fb1_1;
		    fb1_8bit = (unsigned char *)fb1_16bit;
    	}
    }
}

/* Sets layer properties */
static void lc_setlayer(int layer, bool onoff, bool alpha, bool invert, bool trans, unsigned int mode)
{
	/* set layer properties register */
	unsigned int temp;
	int pixel_width;
	temp = 0;
	if(onoff)	temp |= BIT(5);
	if(alpha)	temp |= BIT(2);
	if(invert)	temp |= BIT(1);
	if(trans)	temp |= BIT(0);
	temp |= BIT(12);
	temp |= BIT(14);
	temp |= BIT(15);
	if(mode) temp |= (mode<<16);

	if(layer == 0) {
		MLCCONTROL0 = MLC2CONTROL0 = temp;
	} else {
		MLCCONTROL1 = MLC2CONTROL1 = temp;
	}
	lc_dirtylayer(layer);

	/* set stride based on pixel width*/
	switch(mode) {
		case RGB565:
		case BGR565:
		case XRGB1555:
		case XBGR1555:
		case XRGB4444:
		case XBGR4444:
		case XRGB8332:
		case XBGR8332:
		case ARGB1555:
		case ABGR1555:
		case ARGB4444:
		case ABGR4444:
		case ARGB8332:
		case ABGR8332:
			pixel_width = 2;
			break;
		case RGB888:
		case BGR888:
			pixel_width = 3;
			break;
		case ARGB8888:
		case ABGR8888:
			pixel_width = 4;
			break;
		case PTRGB565:
			pixel_width = 1;
			break;
		default:
			break;
	}
	lc_setstride(layer, pixel_width, pixel_width*layer_width[layer]);
}

/* Sets layer position */
static void lc_layerpos(int layer, int x1, int y1, int x2, int y2)
{
	unsigned int temp_lr, temp_tb;
	temp_lr = (x1 << 16) | x2;
	temp_tb = (y1 << 16) | y2;

	if(layer == 0) {
		MLCLEFTRIGHT0 = MLC2LEFTRIGHT0 = temp_lr;
		MLCTOPBOTTOM0 = MLC2TOPBOTTOM0 = temp_tb;
	} else {
		MLCLEFTRIGHT1 = MLC2LEFTRIGHT1 = temp_lr;
		MLCTOPBOTTOM1 = MLC2TOPBOTTOM1 = temp_tb;
	}
	lc_dirtylayer(layer);
	
	layer_width[layer] = (x2-x1)+1;
}

/*
static void lc_setalpha(int layer, int value)
{
	if(value < 0 || value > 15)
		return;

	if(layer == 0) {
		MLCTPCOLOR0 = (MLCTPCOLOR0&0xFFFFFF) | value << 28;
	} else {
		MLCTPCOLOR1 = (MLCTPCOLOR1&0xFFFFFF) | value << 28;
	}
	lc_dirtylayer(layer);
}

static void lc_settranscol(int layer, unsigned int colour)
{
	if(layer == 0) {
		MLCTPCOLOR0 = (MLCTPCOLOR0&0xFF000000) | (colour&0xFFFFFF);
	} else {
		MLCTPCOLOR1 = (MLCTPCOLOR1&0xFF000000) | (colour&0xFFFFFF);
	}
	lc_dirtylayer(layer);
}

static void lc_setinvcol(int layer, unsigned int colour)
{
	if(layer == 0) {
		MLCINVCOLOR0 = colour;
	} else {
		MLCINVCOLOR1 = colour;
	}
	lc_dirtylayer(layer);
}
*/

/* Sets the dirty flag for the MLC */
static void lc_dirtymlc(void)
{
	MLCCONTROLT |= BIT(3);
}

/* Sets the dirty flag for the layer */
static void lc_dirtylayer(int layer)
{
	if(layer == 0) {
		MLCCONTROL0 |= BIT(4);
		MLC2CONTROL0 |= BIT(4);
	} else {
		MLCCONTROL1 |= BIT(4);
		MLC2CONTROL1 |= BIT(4);
	}
}

/* Sets the screen size */
static void lc_screensize(int w, int h)
{
	MLCSCREENSIZE = ((h-1)<<16) | (w-1);
	lc_dirtymlc();
}

/* Sets the background colour */
static void lc_setbgcol(unsigned int colour)
{
	/* colour to be displayed where no layers cover */
	MLCBGCOLOR = colour;
	lc_dirtymlc();
}

/* Sets stride registers */
static void lc_setstride(int layer, int hs, int vs)
{
	/* set how many bytes the MLC is supposed to read */
	if(layer == 0) {
		MLCHSTRIDE0 = MLC2HSTRIDE0 = hs;
		MLCVSTRIDE0 = MLC2VSTRIDE0 = vs;
	} else {
		MLCHSTRIDE1 = MLC2HSTRIDE1 = hs;
		MLCVSTRIDE1 = MLC2VSTRIDE1 = vs;
	}
	lc_dirtylayer(layer);
}

int wiz_init(int bpp, int rate, int bits, int stereo)
{
	printf("wiz_init()... ");

	/* open /dev/mem to access registers */
	wiz_dev[0] = open("/dev/mem", O_RDWR);
	if(wiz_dev[0] < 0) {
		printf("Could not open /dev/mem\n");
		return -1;
	}

	/* get access to the registers */
	memregs32 = (volatile unsigned int *)mmap(0, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, wiz_dev[0], 0xC0000000);
	if(memregs32 == (volatile unsigned int *)0xFFFFFFFF) {
		printf("Could not mmap hardware registers\n");
		return -1;
	}
	memregs16 = (volatile unsigned short *)memregs32;
	memregs8 = (volatile unsigned char *)memregs32;

	/* backup old register values to restore upon exit */
	bkregs32[0] = MLCADDRESS0; bkregs32[1] = MLCADDRESS1; bkregs32[2] = MLCCONTROL0; bkregs32[3] = MLCCONTROL1; bkregs32[4] = MLCLEFTRIGHT0;
	bkregs32[5] = MLCTOPBOTTOM0; bkregs32[6] = MLCLEFTRIGHT1; bkregs32[7] = MLCTOPBOTTOM1; bkregs32[8] = MLCBGCOLOR; bkregs32[9] = MLCHSTRIDE0;
	bkregs32[10] = MLCVSTRIDE0; bkregs32[11] = MLCHSTRIDE1; bkregs32[12] = MLCVSTRIDE1; bkregs32[13] = DPCCTRL1; bkregs32[14] = MLCSCREENSIZE;
    
	sec_bkregs32[0] = MLC2ADDRESS0; sec_bkregs32[1] = MLC2ADDRESS1; sec_bkregs32[2] = MLC2CONTROL0; sec_bkregs32[3] = MLC2CONTROL1; sec_bkregs32[4] = MLC2LEFTRIGHT0;
	sec_bkregs32[5] = MLC2TOPBOTTOM0; sec_bkregs32[6] = MLC2LEFTRIGHT1; sec_bkregs32[7] = MLC2TOPBOTTOM1; sec_bkregs32[9] = MLC2HSTRIDE0;
	sec_bkregs32[10] = MLC2VSTRIDE0; sec_bkregs32[11] = MLC2HSTRIDE1; sec_bkregs32[12] = MLC2VSTRIDE1;

	/* Set Wiz Clock */
	wiz_set_clock(wiz_clock);

	/* access framebuffers */
	upper_fb=(unsigned char  *)mmap(0, FBX_L*4, PROT_READ|PROT_WRITE, MAP_SHARED, wiz_dev[0], 0x5600000);

#ifdef MMUHACK
	warm_init();
    warm_change_cb_upper(WCB_C_BIT|WCB_B_BIT, 1);
#endif
	
	uppermem=(unsigned char*)memalign(1024,(24*1024*1024));
	upper_malloc_init(uppermem);

	/* assign framebuffers */
	fb0_0 = (unsigned short *)(upper_fb);
	fb0_1 = (unsigned short *)(upper_fb+FBX_L);
	fb1_0 = (unsigned short *)(upper_fb+FBX_L*2);
	fb1_1 = (unsigned short *)(upper_fb+FBX_L*3);
	
    /* assign initial framebuffers */
	fb0_16bit = fb0_1; fb0_8bit=(unsigned char *)fb0_16bit;
	fb1_16bit = fb1_1; fb1_8bit=(unsigned char *)fb1_16bit;

	/* clear framebuffers */
	memset((void*)fb0_0, 0x00, FBX_L);
	memset((void*)fb0_1, 0x00, FBX_L);
	memset((void*)fb1_0, 0x00, FBX_L);
	memset((void*)fb1_1, 0x00, FBX_L);

    /* set screen orientation */
	lc_screensize(320, 240); 
	lc_setbgcol(0x000000); /* set default background colour */
	lc_layerpos(0, 0, 0, 319, 239);	/* set default layer positions */
	lc_layerpos(1, 0, 0, 319, 239);

	if (bpp==16)
	{
	    lc_setlayer(0, false, false, false, false, RGB565); /* set default layer settings */
	    lc_setlayer(1, true, false, false, false, RGB565);
	}
	else
	{
	    lc_setlayer(0, false, false, false, false, PTRGB565); /* set default layer settings */
	    lc_setlayer(1, true, false, false, false, PTRGB565);
        int i;
        for (i=0; i<256; i++)
        {
            wiz_video_color8(i,0,0,0);
        }
        wiz_video_color8(255,255,255,255);
        wiz_video_setpalette();
	}
	lc_flipfb(0,1);	/* set initial addresses in hardware */
	lc_flipfb(1,1);
	usleep(200000);

	/* open /dev/dsp to access sound card */
  	wiz_dev[1] = open("/dev/dsp",   O_WRONLY);
	if(wiz_dev[1] < 0) {
		printf("Could not open /dev/dsp\n");
		return -1;
	}

    /* set sound settings */
 	if (ioctl(wiz_dev[1], SNDCTL_DSP_SETFMT, &bits)==-1) /* bits */
 	    printf("Error in SNDCTL_DSP_SETFMT\n"); 
  	if (ioctl(wiz_dev[1], SNDCTL_DSP_STEREO, &stereo)==-1) /* stereo */
  	    printf("Error in SNDCTL_DSP_STEREO\n");
    rate=(rate<22050?22050:rate);
	if (ioctl(wiz_dev[1], SNDCTL_DSP_SPEED,  &rate)==-1) /* rate */
	    printf("Error in SNDCTL_DSP_SPEED\n");

    /* Enable RAM tweaks */
    if (wiz_ram_tweaks)
		pollux_set(memregs16, "ram_timings=3,9,4,1,1,1,1");

	// DEFAULT CAANOO RAM TIMINGS
	// printf("RAM: %x %x\n", memregs16[0x14802>>1], memregs16[0x14804>>1]);
	// RAM:        2f22             3843
	// RAM: 0010111100100010 0011100001000011
	// ram_timings[0] -> 3
	// ram_timings[1] -> 8
	// ram_timings[2] -> 4
	// ram_timings[3] -> 3
	// ram_timings[4] -> 2
	// ram_timings[5] -> 2
	// ram_timings[6] -> 2
	// "ram_timings=3,10,6,3,2,2,2"
	
	// EXOPHASE
	// For 133MHz system clock the datasheet says these values (cycles @ 133MHz):
	// CAS latency: 2.5 cycles
	// tRC: 55ns (8 cycles)
	// tRAS: 40ns (6 cycles)
	// tWR: 15ns (2 cycles)
	// tMRD: ???
	// tRP: 15ns (2 cycles)
	// tRCD: 15ns (2 cycles)
	// pollux_set(memregs16, "ram_timings=2.5,8,6,2,2,2,2");
			
    #ifdef WIZ_TIMER
        TIMER_REG(0x44) = 0x922;
        TIMER_REG(0x40) = 0x0c;
        TIMER_REG(0x08) = 0x6b;
    #endif
    
	/* USB Joysticks Initialization */
	usbjoy_init();

	if (gsensor) {
		KIONIX_ACCEL_init();
	
		if (KIONIX_ACCEL_get_device_type() != DEVICE_TYPE_KIONIX_KXTF9) {
			KIONIX_ACCEL_deinit();
			gsensor = 0;
		}

		KXTF9_set_G_range(2);
		KXTF9_set_resolution(12);
		KXTF9_set_lpf_odr(400);

		KIONIX_ACCEL_enable_outputs();
	}

	printf("OK\n");
	return 0;
}

void wiz_deinit(void)
{
	printf("wiz_deinit()... ");

    #ifdef WIZ_TIMER
        TIMER_REG(0x40) = 0x0c;
        TIMER_REG(0x08) = 0x23;
        TIMER_REG(0x00) = 0;
        TIMER_REG(0x40) = 0;
        TIMER_REG(0x44) = 0;
    #endif
    
  	memset(fb1_16bit, 0, FBX_L); wiz_video_flip();
  	memset(fb1_16bit, 0, FBX_L); wiz_video_flip();
	wiz_video_flip_single();

#ifdef MMUHACK
    warm_finish();
#endif

	/* restore old register values */
	MLCADDRESS0 = bkregs32[0]; MLCADDRESS1 = bkregs32[1]; MLCCONTROL0 = bkregs32[2]; MLCCONTROL1 = bkregs32[3]; MLCLEFTRIGHT0 = bkregs32[4];
	MLCTOPBOTTOM0 = bkregs32[5]; MLCLEFTRIGHT1 = bkregs32[6]; MLCTOPBOTTOM1 = bkregs32[7]; MLCBGCOLOR = bkregs32[8]; MLCHSTRIDE0 = bkregs32[9];
	MLCVSTRIDE0 = bkregs32[10]; MLCHSTRIDE1 = bkregs32[11]; MLCVSTRIDE1 = bkregs32[12]; DPCCTRL1 = bkregs32[13]; MLCSCREENSIZE = bkregs32[14];

	MLC2ADDRESS0 = sec_bkregs32[0]; MLC2ADDRESS1 = sec_bkregs32[1]; MLC2CONTROL0 = sec_bkregs32[2]; MLC2CONTROL1 = sec_bkregs32[3]; MLC2LEFTRIGHT0 = sec_bkregs32[4];
	MLC2TOPBOTTOM0 = sec_bkregs32[5]; MLC2LEFTRIGHT1 = sec_bkregs32[6]; MLC2TOPBOTTOM1 = sec_bkregs32[7]; MLC2HSTRIDE0 = sec_bkregs32[9];
	MLC2VSTRIDE0 = sec_bkregs32[10]; MLC2HSTRIDE1 = sec_bkregs32[11]; MLC2VSTRIDE1 = sec_bkregs32[12];

	lc_dirtylayer(0);
	lc_dirtylayer(1);
	lc_dirtymlc();

	if (gsensor) 
		KIONIX_ACCEL_deinit();

	free(uppermem);
	munmap((void *)upper_fb, FBX_L*4);
   	munmap((void *)memregs32, 0x20000);

	/* USB Joysticks Close */
	usbjoy_close();

 	close(wiz_dev[1]);
 	close(wiz_dev[0]);

	fcloseall(); /* close all files */

	printf("OK\n");
}

#define SYS_CLK_FREQ 27
void wiz_set_clock(int speed)
{
	unsigned  long v;
	unsigned mdiv, pdiv=20, sdiv=0;

	mdiv= (speed * pdiv) / SYS_CLK_FREQ;
	mdiv &= 0x3FF;
	v= pdiv<<18 | mdiv<<8 | sdiv;

	PLLSETREG0 = v;
	PWRMODE |= 0x8000;
	while (PWRMODE & 0x8000);
}

#define MAX_EVENTS 64
int abs_x=0, abs_y=0, abs_z=0;

#if 0
void wiz_joystick_read_analog(void)
{
	struct input_event event[MAX_EVENTS];
	int i, bytes; 

	bytes = read(wiz_dev[3], &event, sizeof(event));
	if (bytes <= 0)
		return;

	for (i=0; i<bytes/sizeof(struct input_event); i++) {
		if (event[i].type == EV_ABS) {
			switch (event[i].code) {
				case ABS_X: abs_x = event[i].value - 128; break;
				case ABS_Y: abs_y = event[i].value - 128; break;
			}
		}
	}
}
#endif

unsigned int wiz_key_value(int value)
{
	unsigned int res=0;

	if (value == 0)  { res |= WIZ_A; }
	if (value == 1)  { res |= WIZ_X; }
	if (value == 2)  { res |= WIZ_B; }
	if (value == 3)  { res |= WIZ_Y; }
	if (value == 4)  { res |= WIZ_L; }
	if (value == 5)  { res |= WIZ_R; }
	if (value == 6)  { res |= WIZ_HOME; } /* left shoulder button 2 */
	if (value == 7)  { res |= WIZ_HOLD; } /* right shoulder button 2 */
	if (value == 8)  { res |= WIZ_SELECT; }
	if (value == 9)  { res |= WIZ_MENU; }
	if (value == 10) { res |= WIZ_PUSH; }

	return res;
}

static int hold = 0;

unsigned int wiz_joystick_read(int n)
{
	unsigned int res=0;

	if (hold &&  (GPIOAPAD & WIZ_HOLD)) return 0;
	if (hold && !(GPIOAPAD & WIZ_HOLD)) {
		hold = 0;
		res |= WIZ_HOLD;
	}
	if (!hold && (GPIOAPAD & WIZ_HOLD)) {
		hold = 1;
		res |= WIZ_HOLD;
	}

	res |= usbjoy_check(joys[n]);
	if (n == 0) {
		if (rotate_controls) {
			if (res & WIZ_MENU)   res |= WIZ_B;
			if (res & WIZ_SELECT) res |= WIZ_X;
			if (res & WIZ_PUSH)   res |= WIZ_A;
		}

		if (gsensor) {
			if (!rotate_controls) {
				KIONIX_ACCEL_read_LPF_g(&abs_x, &abs_y, &abs_z);
				abs_x /= -2; abs_x -= 23;
				abs_y -= 500; abs_y /= 2;

				if (abs_x < -32) res |= WIZ_LEFT;
				if (abs_x >  32) res |= WIZ_RIGHT;
				if (abs_y < -32) res |= WIZ_UP;
				if (abs_y >  32) res |= WIZ_DOWN;
			} else {
				KIONIX_ACCEL_read_LPF_g(&abs_y, &abs_x, &abs_z);
				abs_x /= 2;
				abs_y -= 500; abs_y /= 2;

				if (abs_y < -32) res |= WIZ_RIGHT;
				if (abs_y >  32) res |= WIZ_LEFT;
				if (abs_x < -32) res |= WIZ_UP;
				if (abs_x >  32) res |= WIZ_DOWN;
			}
		}
	} else {
		if (rotate_controls) {
			int left  = (res & WIZ_LEFT);
			int right = (res & WIZ_RIGHT);
			int up    = (res & WIZ_UP);
			int down  = (res & WIZ_DOWN);

			res &= ~(WIZ_LEFT | WIZ_RIGHT | WIZ_UP | WIZ_DOWN);

			res |= (left)  ? WIZ_UP    : 0;
			res |= (right) ? WIZ_DOWN  : 0;
			res |= (up)    ? WIZ_RIGHT : 0;
			res |= (down)  ? WIZ_LEFT  : 0;
		}
	}

	return res;
}

void wiz_video_flip(void)
{
    lc_flipfb(1,0);
}

void wiz_video_flip_single(void)
{
    lc_flipfb(1,1);
}

wiz_palette wiz_video_RGB_palette[256];

void wiz_video_setpalette(void)
{
    int i;
    for (i=0; i<256; i++)
    {
        if (wiz_video_RGB_palette[i].dirty)
        {
            MLCPALETTE1 = i<<24 | wiz_video_RGB_palette[i].color;
            MLC2PALETTE1 = i<<24 | wiz_video_RGB_palette[i].color;
            wiz_video_RGB_palette[i].dirty = 0;
        }
    }
    MLCCONTROL1 |= 0x10; // Apply changes
    MLC2CONTROL1 |= 0x10; // Apply changes
	//lc_dirtylayer(1);
}

unsigned int wiz_joystick_press (int n)
{
	unsigned int ExKey=0;
	while(wiz_joystick_read(n)&0xFFFE00) { wiz_timer_delay(150); }
	while(!(ExKey=wiz_joystick_read(n)&0xFFFE00)) { wiz_timer_delay(150);}
	return ExKey;
}

void wiz_timer_delay(clock_type ticks)
{
	clock_type ini=wiz_timer_read();
	while (wiz_timer_read()-ini<ticks) { spend_cycles(1024); }
}


clock_type wiz_timer_read(void)
{
    #ifdef WIZ_TIMER
        TIMER_REG(0x08) = 0x4b;  /* run timer, latch value */
        return TIMER_REG(0);
    #else
        return clock(); /* CLOCKS_PER_SEC = 1000000 */
    #endif
}

void wiz_timer_profile(void)
{
	static clock_type i=0;
	if (!i) i=wiz_timer_read();
	else {
		printf("%ld\n",wiz_timer_read()-i);
		i=0;	
	}
}

static pthread_t wiz_sound_thread=0;								// Thread for wiz_sound_thread_play()
static volatile int wiz_sound_thread_exit=0;						// Flag to end wiz_sound_thread_play() thread
static volatile int wiz_sound_buffer=0;							    // Current sound buffer
#define MAX_SAMPLE_RATE (44100*2)
static short wiz_sound_buffers_total[(MAX_SAMPLE_RATE*16)/30];		// Sound buffer
static void *wiz_sound_buffers[16] = {								// Sound buffers
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*0)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*1)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*2)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*3)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*4)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*5)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*6)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*7)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*8)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*9)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*10)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*11)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*12)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*13)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*14)/30)),
	(void *)(wiz_sound_buffers_total+((MAX_SAMPLE_RATE*15)/30))
};
static volatile int sndlen=(MAX_SAMPLE_RATE*2)/60;						// Current sound buffer length

static __inline void stereo_11_to_22khz(int *dest,int *orig, int len)    { do {  *dest++=*orig; *dest++=*orig++; } while (--len); }
static __inline void mono_11_to_22khz(short *dest,short *orig, int len)  { do { *dest++=*orig; *dest++=*orig++; } while (--len); }
static __inline void stereo_16_to_22khz(int *dest,int *orig, int len)    { len=len>>1; do { *dest++=*orig; *dest++=*orig++; *dest++=*orig++; } while (--len); }
static __inline void mono_16_to_22khz(short *dest,short *orig, int len)  { len=len>>1; do { *dest++=*orig; *dest++=*orig++; *dest++=*orig++; } while (--len); }

void wiz_sound_play(void *buff, int len)
{
	int nbuff=(wiz_sound_buffer+1)&15; // Sound buffer to write

    // Convert 11 KHz to 22 KHz
    if (wiz_sound_rate==11025)
    {
        if (wiz_sound_stereo)
            stereo_11_to_22khz((int*)wiz_sound_buffers[nbuff],(int*)buff,len>>2);
        else
            mono_11_to_22khz((short*)wiz_sound_buffers[nbuff],(short*)buff,len>>1);
        len=(len<<1);
    }
    // Convert 15 KHz to 22 KHz
    else if (wiz_sound_rate==15000)
    {
        if (wiz_sound_stereo)
            stereo_16_to_22khz((int*)wiz_sound_buffers[nbuff],(int*)buff,len>>2);
        else
            mono_16_to_22khz((short*)wiz_sound_buffers[nbuff],(short*)buff,len>>1);
        len=(len>>1)*3;
    }
    // Direct copy
    else
    {
    	memcpy(wiz_sound_buffers[nbuff],buff,len); // Write the sound buffer
    }
   	wiz_sound_buffer=nbuff; // Update the current sound buffer
   	sndlen=len;	// Update the sound buffer length
}

void wiz_sound_thread_mute(void)
{
	memset(wiz_sound_buffers_total,0,(MAX_SAMPLE_RATE*16*2)/30);
	sndlen=(wiz_sound_rate*2)/60;
}

static void *wiz_sound_thread_play(void *none)
{
	int nbuff=wiz_sound_buffer;								            // Number of the sound buffer to play
	do {
		write(wiz_dev[1], wiz_sound_buffers[nbuff], sndlen); 			// Play the sound buffer
		ioctl(wiz_dev[1], SOUND_PCM_SYNC, 0);						    // Synchronize Audio
		nbuff=(nbuff+(nbuff!=wiz_sound_buffer))&15;					    // Update the sound buffer to play
	} while(!wiz_sound_thread_exit);							        // Until the end of the sound thread
	pthread_exit(0);
}

void wiz_sound_thread_start(void)
{
	if (!wiz_sound_thread)
	{
		wiz_sound_thread_mute();
		wiz_sound_thread_exit=0;
		wiz_sound_buffer=0;
		wiz_sound_set_stereo(wiz_sound_stereo);
		wiz_sound_set_rate(wiz_sound_rate);
		sndlen=(wiz_sound_rate*2)/60;
		pthread_create( &wiz_sound_thread, NULL, wiz_sound_thread_play, NULL);
	}
}

void wiz_sound_thread_stop(void)
{
	if (wiz_sound_thread)
	{
		wiz_sound_thread_mute();
		wiz_sound_thread_exit=1;
		wiz_timer_delay(500);
		wiz_sound_thread=0;
	}
}

void wiz_sound_set_rate(int rate)
{
    rate=(rate<22050?22050:rate);
	if (ioctl(wiz_dev[1], SNDCTL_DSP_SPEED,  &rate)==-1)
	    printf("Error in SNDCTL_DSP_SPEED\n");
}

void wiz_sound_set_stereo(int stereo)
{
  	if (ioctl(wiz_dev[1], SNDCTL_DSP_STEREO, &stereo)==-1)
  	    printf("Error in SNDCTL_DSP_STEREO\n");
}

void wiz_set_video_mode(int bpp,int width,int height)
{
    if (wiz_rotated_video)
    {
        width=240;
        height=320;
    }
    else
    {
        width=320;
        height=240;
    }
    
  	memset(fb1_16bit, 0, FBX_L); wiz_video_flip();
  	memset(fb1_16bit, 0, FBX_L); wiz_video_flip();
	wiz_video_flip_single();

    /* set screen orientation */
	lc_screensize(width, height); 
	lc_setbgcol(0x000000); /* set default background colour */
	lc_layerpos(0, 0, 0, width-1, height-1); /* set default layer positions */
	lc_layerpos(1, 0, 0, width-1, height-1);
	
	if (bpp==16)
	{
	    lc_setlayer(0, false, false, false, false, RGB565); /* set default layer settings */
	    lc_setlayer(1, true, false, false, false, RGB565);
	}
	else
	{
	    lc_setlayer(0, false, false, false, false, PTRGB565); /* set default layer settings */
	    lc_setlayer(1, true, false, false, false, PTRGB565);
        int i;
        for (i=0; i<256; i++)
        {
            wiz_video_color8(i,0,0,0);
        }
        wiz_video_color8(255,255,255,255);
        wiz_video_setpalette();
	}
	lc_flipfb(0,1);	/* set initial addresses in hardware */
	lc_flipfb(1,1);
	usleep(200000);
}

static const unsigned char fontdata8x8[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x3C,0x42,0x99,0xBD,0xBD,0x99,0x42,0x3C,0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C,
	0xFE,0x82,0x8A,0xD2,0xA2,0x82,0xFE,0x00,0xFE,0x82,0x82,0x82,0x82,0x82,0xFE,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x80,0xC0,0xF0,0xFC,0xF0,0xC0,0x80,0x00,0x01,0x03,0x0F,0x3F,0x0F,0x03,0x01,0x00,
	0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0x00,0xEE,0xEE,0xEE,0xCC,0x00,0xCC,0xCC,0x00,
	0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x3C,0x66,0x7A,0x7A,0x7E,0x7E,0x3C,0x00,0x0E,0x3E,0x3A,0x22,0x26,0x6E,0xE4,0x40,
	0x18,0x3C,0x7E,0x3C,0x3C,0x3C,0x3C,0x00,0x3C,0x3C,0x3C,0x3C,0x7E,0x3C,0x18,0x00,
	0x08,0x7C,0x7E,0x7E,0x7C,0x08,0x00,0x00,0x10,0x3E,0x7E,0x7E,0x3E,0x10,0x00,0x00,
	0x58,0x2A,0xDC,0xC8,0xDC,0x2A,0x58,0x00,0x24,0x66,0xFF,0xFF,0x66,0x24,0x00,0x00,
	0x00,0x10,0x10,0x38,0x38,0x7C,0xFE,0x00,0xFE,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x1C,0x18,0x00,0x18,0x18,0x00,
	0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x7C,0x28,0x7C,0x28,0x00,0x00,
	0x10,0x38,0x60,0x38,0x0C,0x78,0x10,0x00,0x40,0xA4,0x48,0x10,0x24,0x4A,0x04,0x00,
	0x18,0x34,0x18,0x3A,0x6C,0x66,0x3A,0x00,0x18,0x18,0x20,0x00,0x00,0x00,0x00,0x00,
	0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x00,0x0C,0x06,0x06,0x06,0x06,0x06,0x0C,0x00,
	0x10,0x54,0x38,0x7C,0x38,0x54,0x10,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
	0x38,0x4C,0xC6,0xC6,0xC6,0x64,0x38,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x7C,0xC6,0x0E,0x3C,0x78,0xE0,0xFE,0x00,0x7E,0x0C,0x18,0x3C,0x06,0xC6,0x7C,0x00,
	0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0xFC,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,
	0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00,
	0x78,0xC4,0xE4,0x78,0x86,0x86,0x7C,0x00,0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,
	0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30,
	0x1C,0x38,0x70,0xE0,0x70,0x38,0x1C,0x00,0x00,0x7C,0x00,0x00,0x7C,0x00,0x00,0x00,
	0x70,0x38,0x1C,0x0E,0x1C,0x38,0x70,0x00,0x7C,0xC6,0xC6,0x1C,0x18,0x00,0x18,0x00,
	0x3C,0x42,0x99,0xA1,0xA5,0x99,0x42,0x3C,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,
	0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,
	0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00,
	0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00,0x3E,0x60,0xC0,0xCE,0xC6,0x66,0x3E,0x00,
	0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0xC6,0xCC,0xD8,0xF0,0xF8,0xDC,0xCE,0x00,
	0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00,
	0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xCC,0x7A,0x00,
	0xFC,0xC6,0xC6,0xCE,0xF8,0xDC,0xCE,0x00,0x78,0xCC,0xC0,0x7C,0x06,0xC6,0x7C,0x00,
	0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xC6,0xC6,0xC6,0xEE,0x7C,0x38,0x10,0x00,0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00,
	0xC6,0xEE,0x3C,0x38,0x7C,0xEE,0xC6,0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
	0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,
	0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,
	0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x66,0x3C,0x00,
	0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
	0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x3C,0x00,
	0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x60,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,
	0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,
	0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0xEC,0xFE,0xFE,0xFE,0xD6,0xC6,0x00,
	0x00,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
	0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x06,
	0x00,0x7E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x3C,0x60,0x3C,0x06,0x66,0x3C,0x00,
	0x30,0x78,0x30,0x30,0x30,0x30,0x1C,0x00,0x00,0x66,0x66,0x66,0x66,0x6E,0x3E,0x00,
	0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x7C,0x6C,0x00,
	0x00,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,0x0E,0x18,0x0C,0x38,0x0C,0x18,0x0E,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x30,0x1C,0x30,0x18,0x70,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x10,0x28,0x10,0x54,0xAA,0x44,0x00,0x00,
};

static void wiz_text(unsigned char *screen, int x, int y, char *text, int color)
{
	unsigned int i,l;
	screen=screen+x+y*320;

	for (i=0;i<strlen(text);i++) {
		
		for (l=0;l<8;l++) {
			screen[l*320+0]=(fontdata8x8[((text[i])*8)+l]&0x80)?color:screen[l*320+0];
			screen[l*320+1]=(fontdata8x8[((text[i])*8)+l]&0x40)?color:screen[l*320+1];
			screen[l*320+2]=(fontdata8x8[((text[i])*8)+l]&0x20)?color:screen[l*320+2];
			screen[l*320+3]=(fontdata8x8[((text[i])*8)+l]&0x10)?color:screen[l*320+3];
			screen[l*320+4]=(fontdata8x8[((text[i])*8)+l]&0x08)?color:screen[l*320+4];
			screen[l*320+5]=(fontdata8x8[((text[i])*8)+l]&0x04)?color:screen[l*320+5];
			screen[l*320+6]=(fontdata8x8[((text[i])*8)+l]&0x02)?color:screen[l*320+6];
			screen[l*320+7]=(fontdata8x8[((text[i])*8)+l]&0x01)?color:screen[l*320+7];
		}
		screen+=8;
	} 
}

void wiz_gamelist_text_out(int x, int y, char *eltexto)
{
	char texto[33];
	strncpy(texto,eltexto,32);
	texto[32]=0;
	if (texto[0]!='-')
		wiz_text(fb1_8bit,x+1,y+1,texto,0);
	wiz_text(fb1_8bit,x,y,texto,255);
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void wiz_gamelist_text_out_fmt(int x, int y, char* fmt, ...)
{
	char strOut[128];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	wiz_gamelist_text_out(x, y, strOut);
}

static int log=0;

void wiz_printf_init(void)
{
	log=0;
}

static void wiz_text_log(char *texto)
{
	if (!log)
	{
		memset(fb1_8bit,0,320*240);
	}
	wiz_text(fb1_8bit,0,log,texto,255);
	log+=8;
	if(log>239) log=0;
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void wiz_printf(char* fmt, ...)
{
	int i,c;
	char strOut[4096];
	char str[41];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	c=0;
	for (i=0;i<strlen(strOut);i++)
	{
		str[c]=strOut[i];
		if (str[c]=='\n')
		{
			str[c]=0;
			wiz_text_log(str);
			c=0;
		}
		else if (c==39)
		{
			str[40]=0;
			wiz_text_log(str);
			c=0;
		}		
		else
		{
			c++;
		}
	}
}

void wiz_video_wait_vsync(void)
{
  while((DPCCTRL0 & (1 << 10)) == 0);
  DPCCTRL0 |= (1 << 10);
}
