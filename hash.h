#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

bool isSame(unsigned char a[], unsigned char b[], int len);
unsigned char * isUniform(unsigned char data[], int len);
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber);

#endif