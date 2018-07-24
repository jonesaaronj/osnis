#include <stdbool.h>
#include "partition.h"

bool IsUniform(unsigned char data[], int offset, int size, unsigned char value) {
    for (int i = 0; i < size; i++) {
        if (data[i + offset] != value) {
            return false;
        }
    }
    return true;
}