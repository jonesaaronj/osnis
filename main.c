#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hash.h"
#include "partition.h"

/**
 * Create a shrunken image from the input file and disc info
 */
void createShrunkImage(struct DiscInfo * discInfo, char *inputFile, char *outputFile) {

    if (!discInfo->isGC && !discInfo->isWII) {
        fprintf(stderr, "ERROR: We are not a GC or WII disc\n");
        return;
    }

    // if file pointer is empty read from stdin
    FILE *inputF = (inputFile != NULL) ? fopen(inputFile, "rb") : stdin;

    // if file pointer is empty read from stdout
    FILE *outputF = (outputFile != NULL) ? fopen(outputFile, "wb") : stdout;

    // Do all of our reading in 0x40000 byte blocks
    unsigned char buffer[BLOCK_SIZE];
    size_t read;
    size_t write;

    int blockNum = discInfo->isGC ? GC_BLOCK_SIZE :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_BLOCK_SIZE : WII_BLOCK_SIZE;

    size_t lastBlockSize = discInfo->isGC ? GC_LAST_BLOCK_SIZE :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_LAST_BLOCK_SIZE : WII_LAST_BLOCK_SIZE;

    // write partition block
    fwrite(discInfo->table, BLOCK_SIZE, 1, outputF);

    // our first block in the table is the magic number so to make things
    // easier start at i = 1
    size_t shrunkBlockNum = 2;
    for(uint64_t i = 1; i <= blockNum; i++) {

        // set the block size to write
        size_t writeSize = i == blockNum ? lastBlockSize : BLOCK_SIZE;

        // read the current block into the buffer
        read = fread(buffer, 1, BLOCK_SIZE, inputF);
        if (read != writeSize) {
            fprintf(stderr, "ERROR: read size not equal to write size\n");
            break;
        }

        // if we are at a zero block in our table we done f'ed up
        if (memcmp(&ZERO, discInfo->table + (i * 8), 8) == 0) {
            fprintf(stderr, "ERROR: Saw a zero block before end of file\n");
            fprintf(stderr, "%d\n", i);
            break;
        }

        // ignore junk blocks
        unsigned char * junk = getJunkBlock(i, discInfo->discId, discInfo->discNumber);
        if (isSame(buffer, junk, read)) {
            // make sure our table is correct
            if (memcmp(&FFs, discInfo->table + (i * 8), 8) == 0) {
                continue;
            } else {
                fprintf(stderr, "ERROR: Saw a junk block but expected something else\n");
                break;
            }
        }

        // if we got this far we should be a data block
        // make sure our table is correct
        if (memcmp(&shrunkBlockNum, discInfo->table + (i * 8), 8) == 0) {
            shrunkBlockNum++;
            // write the block to the file
            write = fwrite(buffer, 1, writeSize, outputF);
            if (write < 0) {
                fprintf(stderr, "ERROR: could not write block\n");
                break;
            }
        } else {
            uint64_t address =  ((uint64_t) discInfo->table[(i * 8) + 7] << 0) +
                                ((uint64_t) discInfo->table[(i * 8) + 6] << 8) +
                                ((uint64_t) discInfo->table[(i * 8) + 5] << 16) +
                                ((uint64_t) discInfo->table[(i * 8) + 4] << 24) +
                                ((uint64_t) discInfo->table[(i * 8) + 3] << 32) +
                                ((uint64_t) discInfo->table[(i * 8) + 2] << 40) +
                                ((uint64_t) discInfo->table[(i * 8) + 1] << 48) +
                                ((uint64_t) discInfo->table[(i * 8) + 0] << 56);

            fprintf(stderr, "ERROR: Saw a data block but address was wrong\n");
            fprintf(stderr, "%d %d\n", shrunkBlockNum, address);
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    char *inputFile = NULL;
    char *outputFile = NULL;
    bool doProfile = false;

    int opt;
    while ((opt = getopt(argc, argv, "i:o:pq")) != -1) {
        switch (opt) {
            case 'p':
                doProfile = true;
                break;
            case 'i':
                inputFile = optarg; 
                break;
            case 'o': 
                outputFile = optarg;
                break;
            case '?':
                if (optopt == 'i' || optopt == 'i') {
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                }            
            default:
                fprintf(stderr, "Usage: %s [-i inputFile] [-o outputFile]\n", argv[0]);
                return 1;
            }
    }

    if (doProfile) {
        struct DiscInfo * discInfo = profileImage(inputFile);
        printDiscInfo(discInfo);
    } else {
        // Creating a shrunken image will take two passes.
        // One to prifile the disc and one to do the write the shrunken image
        struct DiscInfo * discInfo = profileImage(inputFile);
        createShrunkImage(discInfo, inputFile, outputFile);
    }
}
