#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

/**
 * Determine if the two char arrays are the same for lenth
 * 
 * param - unsigned char a[]
 * param - unsigned char b[]
 * param - int length
 *
 * returns - true if the two arrays are the sames
 */
bool isSame(unsigned char a[], unsigned char b[], int length);

/**
 * Get a junk block of size 262144/0X40000 for the given block, disc id, and disc number
 *
 * param - unsigned int blockCount
 * param - unsigned char id[]
 * param - unsigned char discNumber
 *
 * returns - an unsigned char pointer to a 262144/0X40000 block of junk data
 */
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber);

#endif