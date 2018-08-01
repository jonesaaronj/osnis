#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

#define BLOCK_SIZE 0x40000

void printChar(unsigned char *a, int length);

/**
 * Determine if the char array is fillied with the same char for a given length
 */
unsigned char * isUniform(unsigned char *a, int length);

/**
 * Determine if the two char arrays are the same for a given lenth
 */
bool isSame(unsigned char * a, unsigned char * b, int length);

/**
 * Get a junk block of size 262144/0X40000 for the given block, disc id, and disc number
 */
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber);

#endif