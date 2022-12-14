#include "driver.h"
#include "unzip.h"
#include <unistd.h>

#define MAXPATHC 20 /* at most 20 path entries */
#define MAXPATHL 256 /* at most 255 character path length */

char buf1[MAXPATHL];
char buf2[MAXPATHL];

char *romdir = "ROMS";
char *sampledir = "SAMPLES";
char *cfgdir = "CFG";
char *hidir = "HI";
char *inpdir = "INP";
char *stadir = "STA";
char *artworkdir = "ARTWORK";

typedef enum
{
	kPlainFile,
	kRAMFile,
	kZippedFile
} eFileType;

typedef struct
{
	/*GP32_FILE*/	long *file;
	unsigned char	*data;
	unsigned int	offset;
	unsigned int	length;
	eFileType	type;
	unsigned int	crc;
} FakeFileHandle;


static unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
static int checksum_file(const char* file, unsigned char **p, unsigned int *size, unsigned int* crc);

int cache_stat_old(char *name) {

    unsigned int i;
    ulong num_entries;
    ulong read_count;
    GPDIRENTRY * direntry;
    int found=0;

    if ( GpDirEnumNum("gp:\\gpmm\\mame034\0", &num_entries)!=SM_OK )
      return 1;

    if (num_entries==0)
      return 1;

    direntry = gm_zi_malloc ( ((ulong)sizeof(GPDIRENTRY))*num_entries );
    GpDirEnumList ( "gp:\\gpmm\\mame034\0", 0, num_entries, direntry, &read_count );
    num_entries = read_count;

    gm_lowercase(name,gm_lstrlen(name));
    for (i=0;i<num_entries;i++) {
    	gm_lowercase(direntry[i].name,gm_lstrlen(direntry[i].name));
	if (gm_compare(name,direntry[i].name)==0) {
	     found=1;break;
	}
    }

    gm_free ( direntry );

    if (found)
    	return 0;

    return 1;
    
}	

int cache_stat(char *name) {

    char filename[128];
    unsigned int i;
    int found=0;

    sprintf(filename,"gp:\\gpmm\\mame034\\%s\0",name);

    if (gp32_fexists(filename))
    	return 0;
    else
    	return 1;
   
}	

int cache_stat_dummy(char *name) {
	return 0;
}


/*
 * file handling routines
 *
 * gamename holds the driver name, filename is only used for ROMs and samples.
 * if 'write' is not 0, the file is opened for write. Otherwise it is opened
 * for read.
 */

/* JB 980920 update */
/* AM 980919 update */
void *osd_fopen(const char *game,const char *filename,int filetype,int _write)
{
	char name[256];
	char *gamename;
	int found = 0;
	FakeFileHandle *f;

	f = (FakeFileHandle *)gm_zi_malloc(sizeof(FakeFileHandle));
	if (!f)
		return 0;
	gm_memset(f,0,sizeof(FakeFileHandle));

	gamename = (char *)game;

	switch (filetype)
	{
		case OSD_FILETYPE_ROM:

			/* only for reading */
			if (_write)
				break;

			if (!found) {
				if (cache_stat(gamename)==0) {
					sprintf(name,"gp:\\gpmm\\mame034\\%s\\%s\0",gamename,filename);
					if (checksum_file (name, &f->data, &f->length, &f->crc)==0) {
						f->type = kRAMFile;
						f->offset = 0;
						found = 1;
					}
				}
			}

			if (!found) {
				/* try with a .zip extension */
				sprintf(name,"%s.zip\0", gamename);
				if (cache_stat(name)==0) {
					sprintf(name,"gp:\\gpmm\\mame034\\%s.zip\0", gamename);
					if (load_zipped_file(name, filename, &f->data, &f->length)==0) {
						f->type = kZippedFile;
						f->offset = 0;
						f->crc = crc32 (0L, f->data, f->length);
						found = 1;
					}
				}
			}

			break;
		case OSD_FILETYPE_SAMPLE:
			break;
		case OSD_FILETYPE_HIGHSCORE:
			break;
		case OSD_FILETYPE_CONFIG:
			break;
		case OSD_FILETYPE_INPUTLOG:
			break;
		case OSD_FILETYPE_STATE:
			break;
		case OSD_FILETYPE_ARTWORK:
			break;
	}

	if (!found) {
		gm_free(f);
		return 0;
	}

	return f;
}

/* JB 980920 update */
int osd_fread(void *file,void *buffer,int length)
{
	FakeFileHandle *f = (FakeFileHandle *)file;

	switch (f->type)
	{
		case kPlainFile:
			return gp32_fread(buffer,1,length,f->file);
			break;
		case kZippedFile:
		case kRAMFile:
			/* reading from the RAM image of a file */
			if (f->data)
			{
				if (length + f->offset > f->length)
					length = f->length - f->offset;
				gm_memcpy(buffer, f->offset + f->data, length);
				f->offset += length;
				return length;
			}
			break;
	}

	return 0;
}

int osd_fread_swap(void *file,void *buffer,int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	res = osd_fread(file,buffer,length);

	buf = buffer;
	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	return res;
}



/* JB 980920 update */
int osd_fseek(void *file,int offset,int whence)
{
	FakeFileHandle *f = (FakeFileHandle *)file;
	int err = 0;

	switch (f->type)
	{
		case kPlainFile:
			return gp32_fseek(f->file,offset,whence);
			break;
		case kZippedFile:
		case kRAMFile:
			/* seeking within the RAM image of a file */
			switch (whence)
			{
				case SEEK_SET:
					f->offset = offset;
					break;
				case SEEK_CUR:
					f->offset += offset;
					break;
				case SEEK_END:
					f->offset = f->length + offset;
					break;
			}
			break;
	}

	return err;
}

/* JB 980920 update */
void osd_fclose(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch(f->type)
	{
		case kPlainFile:
			gp32_fclose(f->file);
			break;
		case kZippedFile:
		case kRAMFile:
			if (f->data)
				gm_free(f->data);
			break;
	}
	gm_free(f);
}

/* JB 980920 update */
/* AM 980919 */
static int checksum_file(const char* file, unsigned char **p, unsigned int *size, unsigned int *crc) {
	int length;
	unsigned char *data;
	GP32_FILE* f;

	f = gp32_fopen(file,"rb");
	if (!f) {
		return -1;
	}

	/* determine length of file */
	if (gp32_fseek (f, 0L, SEEK_END)!=0) {
		gp32_fclose(f);
		return -1;
	}

	length = gp32_ftell(f);
	if (length == -1L) {
		gp32_fclose(f);
		return -1;
	}

	/* allocate space for entire file */
	data = (unsigned char*)gm_zi_malloc(length);
	if (!data) {
		gp32_fclose(f);
		return -1;
	}

	/* read entire file into memory */
	if (gp32_fseek(f, 0L, SEEK_SET)!=0) {
		gm_free(data);
		gp32_fclose(f);
		return -1;
	}

	if (gp32_fread(data, sizeof (unsigned char), length, f) <length /*FRANXIS 22-01-2005 != length*/) {
		gm_free(data);
		gp32_fclose(f);
		return -1;
	}

	*size = length;
	*crc = crc32 (0L, data, length);
	if (p)
		*p = data;
	else
		gm_free(data);

	gp32_fclose(f);

	return 0;
}

/* JB 980920 updated */
/* AM 980919 updated */
int osd_fchecksum (const char *game, const char *filename, unsigned int *length, unsigned int *sum)
{
	char name[256];
	int found = 0;
	const char *gamename = game;

	if (!found) {
		if (cache_stat((char *)gamename)==0) {
			sprintf(name,"gp:\\gpmm\\mame034\\%s\\%s\0",gamename,filename);
			if (checksum_file(name,0,length,sum)==0) {
				found = 1;
			}
		}
	}

	if (!found) {
		/* try with a .zip extension */
		sprintf(name,"%s.zip\0", gamename);
		if (cache_stat(name)==0) {
			sprintf(name,"gp:\\gpmm\\mame034\\%s.zip\0", gamename);
			if (checksum_zipped_file (name, filename, length, sum)==0) {
				found = 1;
			}
		}
	}

	if (!found)
		return -1;

	return 0;
}

/* JB 980920 */
int osd_fsize (void *file)
{
	FakeFileHandle	*f = (FakeFileHandle *)file;

	if (f->type==kRAMFile || f->type==kZippedFile)
		return f->length;

	return 0;
}

/* JB 980920 */
unsigned int osd_fcrc (void *file)
{
	FakeFileHandle	*f = (FakeFileHandle *)file;

	return f->crc;
}

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
static unsigned int crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len)
{
    if (buf == 0) return 0L;
    crc = crc ^ 0xffffffffL;
    while (len >= 8)
    {
      DO8(buf);
      len -= 8;
    }
    if (len) do {
      DO1(buf);
    } while (--len);
    return crc ^ 0xffffffffL;
}
