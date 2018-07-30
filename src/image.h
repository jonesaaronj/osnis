#ifndef IMAGE_H
#define IMAGE_H

#include "disc_info.h"

/**
 * Unshrink a shrunken image
 */
void unshrinkImage(char *inputFile, char *outputFile);

/**
 * Create a shrunken image from the input file and disc info
 */
void shrinkImage(struct DiscInfo * discInfo, char *inputFile, char *outputFile);

#endif