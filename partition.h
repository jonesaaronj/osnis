#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>

struct disc_info
{
    unsigned char *id;
    unsigned char disc_number;
    bool isGCM;
    bool isWII;
};

struct disc_info * getDiscInfo(unsigned char data[]);

#endif