#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

static const size_t BLOCK_SIZE = 0x40000;

/**
 * Determine if the two char arrays are the same for a given lenth
 */
bool isSame(unsigned char * a, unsigned char * b, int length);

/**
 * Get a junk block of size 262144/0X40000 for the given block, disc id, and disc number
 */
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber);

#endif