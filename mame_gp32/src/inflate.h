#ifndef _MAME_INFLATE_H
#define	_MAME_INFLATE_H

#include "gp32_fileio.h"

/***************************************************************************
 * Support for inflate data
 ***************************************************************************/

/* Inflate a file
   in:
	in_file stream to inflate
	in_size size of the compressed data to read
	out_size size of decompressed data
   out:
	out_data buffer for decompressed data
   return:
	==0 ok
*/
int inflate_file(GP32_FILE* in_file, unsigned in_size, unsigned char* out_data, unsigned out_size);

/* Inflate a memory region
   in:
	in_data data to inflate
	in_size size of the compressed data to read
	out_size size of decompressed data
   out:
	out_data buffer for decompressed data
   return:
	==0 ok
*/
int inflate_memory(const unsigned char* in_data, unsigned in_size, unsigned char* out_data, unsigned out_size);

#endif
