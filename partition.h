#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>

struct gcm_info
{
    unsigned char *id;
    unsigned char disc_number;
    unsigned char *magic_word;
};

bool IsGCM(unsigned char data[]);
struct gcm_info * getGcmInfo(unsigned char data[]);

#endif