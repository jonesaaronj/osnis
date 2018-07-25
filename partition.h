#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>

struct gcm_info
{
    unsigned char *id;
    unsigned char disc_number;
};

bool IsGCM(unsigned char data[]);
struct gcm_info * getGcmInfo(unsigned char data[]);
bool IsUniform(unsigned char data[], int offset, int size, unsigned char value);

#endif