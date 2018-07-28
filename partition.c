#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "partition.h"

/*
 * The magic number starts at byte 28 for GC games
 */
bool IsGC(unsigned char data[])
{
	unsigned char magic_word[] = {0xC2, 0x33, 0x9F, 0x3D};
    return memcmp(data + 28, magic_word, 4) == 0;
}


/*
 * The magic number starts at byte 24 for WII games
 */
bool IsWII(unsigned char data[])
{
    unsigned char magic_word[] = {0x5D, 0x1C, 0x9E, 0xA3};
    return memcmp(data + 24, magic_word, 4) == 0;
}

/**
 * Get the disc info
 */
struct disc_info * getDiscInfo(unsigned char data[])
{
	struct disc_info *disc_info = malloc(sizeof(struct disc_info));

	// the disc id comes from bytes 0 through 5
    unsigned char *id = malloc(sizeof(*id) * 6);
    memcpy(id, data, 6);
	disc_info->id = id;

	// the disc number comes from byte 6
	disc_info->disc_number = data[6];

    disc_info->isGC = IsGC(data);
    disc_info->isWII = IsWII(data);

    // create a partition table
    unsigned char *table = malloc(sizeof(*table) * 0x40000);
    disc_info->table = table;

 	return disc_info;
}