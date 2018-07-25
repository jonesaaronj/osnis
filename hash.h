#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

bool same(unsigned char a[], unsigned char b[], int len);
unsigned char * isUniform(unsigned char data[], int len);
unsigned char * getJunkBlock(unsigned int blockCount, unsigned char id[], unsigned char discNumber);
void a10002710(unsigned int sample, unsigned int buffer[]);
void a100026e0(unsigned int buffer[]);

#endif