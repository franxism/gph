/***************************************************************************

  vidhrdw.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static char *rawbitmap;

static int x,y,screen_width;
static int last_command;



static int pending, pending_x, pending_y, pending_color;

void leprechn_graphics_command_w(int offset,int data)
{
    last_command = data;
}

void leprechn_graphics_data_w(int offset,int data)
{
    int direction;

    if (pending)
    {
        tmpbitmap->line[pending_y][pending_x] = Machine->pens[pending_color];
        rawbitmap[pending_y * screen_width + pending_x] = pending_color;
        pending = 0;
    }

    switch (last_command)
    {
    
    case 0x00:
        direction = (data & 0xf0) >> 4;
        switch (direction)
        {
        case 0x00:
        case 0x04:
        case 0x08:
        case 0x0c:
            break;

        case 0x01:
        case 0x09:
            x++;
            break;

        case 0x02:
        case 0x06:
            y++;
            break;

        case 0x03:
            x++;
            y++;
            break;

        case 0x05:
        case 0x0d:
            x--;
            break;

        case 0x07:
            x--;
            y++;
            break;

        case 0x0a:
        case 0x0e:
            y--;
            break;

        case 0x0b:
            x++;
            y--;
            break;

        case 0x0f:
            x--;
            y--;
            break;
        }

        x = x & 0xff;
        y = y & 0xff;

        pending = 1;
        pending_x = x;
        pending_y = y;
        pending_color = data & 0x0f;

        return;

    
    case 0x08:
        x = data;
        return;

    
    case 0x10:
        y = data;
        return;

    
    case 0x18:
        fillbitmap(tmpbitmap,Machine->pens[data],0);
        gm_memset(rawbitmap, data, screen_width * Machine->drv->screen_height);
        return;
    }
}


int leprechn_graphics_data_r(int offset)
{
    return rawbitmap[y * screen_width + x];
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
int leprechn_vh_start(void)
{
    screen_width = Machine->drv->screen_width;

    if ((rawbitmap = gm_zi_malloc(screen_width*Machine->drv->screen_height)) == 0)
    {
        return 1;
    }

    if ((tmpbitmap = osd_new_bitmap(screen_width,Machine->drv->screen_height,Machine->scrbitmap->depth)) == 0)
    {
        gm_free(rawbitmap);
        return 1;
    }

    pending = 0;

    return 0;
}

/***************************************************************************

  Stop the video hardware emulation.

***************************************************************************/
void leprechn_vh_stop(void)
{
    gm_free(rawbitmap);
    osd_free_bitmap(tmpbitmap);
}


/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/
void leprechn_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh)
{
    copybitmap(bitmap,tmpbitmap,0,0,0,0,&Machine->drv->visible_area,TRANSPARENCY_NONE,0);
}
