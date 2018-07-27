#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "partition.h"

/*
 * The magic number starts at byte 26
 */
bool IsGCM(unsigned char data[])
{
	unsigned char magic_word[] = {0xC2, 0x33, 0x9F, 0x3D};
    return memcmp(data, magic_word, 4) == 0;
}

struct gcm_info * getGcmInfo(unsigned char data[])
{
	struct gcm_info *gcmInfo = malloc(sizeof(struct gcm_info));

	// the disc id comes from bytes 0 through 5
    unsigned char *id = malloc(sizeof(*id) * 6);
    memcpy(id, data, 6);
	gcmInfo->id = id;

	// the disc number comes from byte 7
	gcmInfo->disc_number = data[6];

    // the magic number comes from byte 28-31
    unsigned char *mw = malloc(sizeof(*mw) * 4);
    memcpy(mw, data + 28, 4);
    gcmInfo->magic_word = mw;

 	return gcmInfo;
}