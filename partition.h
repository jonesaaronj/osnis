#ifndef PARTITION_H
#define PARTITION_H

#include <stdbool.h>

struct disc_info
{
    unsigned char * id;
    unsigned char disc_number;
    unsigned char * disc_name;
    bool isGC;
    bool isWII;
    bool isDualLayer;
    unsigned char * table;
};

struct disc_info * getDiscInfo(unsigned char data[]);

#endif
