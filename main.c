#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hash.h"
#include "partition.h"

const unsigned char GC_DISC = 1;
const unsigned char WII_DISC = 2;
const unsigned char WII_DL_DISC = 3;

/*
 * Profile a disk.  Expects a full iso with valid 
 * disc id and magic number
 */
struct disc_info * profile(char *file)
{
    FILE *f;
    
    // if file pointer is empty read from stdin
    f = (file != NULL) ? fopen(file, "rb") : stdin;

    // Do all of our reading in 0x40000 byte blocks
    unsigned char buffer[0x40000];
    unsigned int i = 0;
    size_t read;

    // keep track of how much data or junk we see
    int dataCount = 0;
    int junkCount = 0;
    int uniformCount = 0;
    unsigned char uniformByte = 0;

    struct disc_info * discInfo = NULL;
    
    // force our blockNum to be an unsigned 64 bit int (8 bytes * 8 bits)
    // to make copying to the partition table easier
    uint64_t FFs = 0xFFFFFFFF;
    uint64_t blockNum = 0;
    while((read = fread(buffer, 1, 0x40000, f)) > 0) {

        // get the disc info from the first block
        if (discInfo == NULL) {
            discInfo = getDiscInfo(buffer);

            if (!discInfo->isGC || !discInfo->isWII) {
                printf("ERROR: We are not a GC or WII disc\n");
                break;
            }

            // set disc info in partition table
            memcpy(discInfo->table, discInfo->id, 6);
            // set the disc number in the partition table
            memcpy(discInfo->table + 6, &discInfo->disc_number, 1);

            // don't forget to set the disc type at the end of everything
            blockNum++;
            continue;
        }

        // get the junk block for this block number
        unsigned char * junk = getJunkBlock(i++, discInfo->id, discInfo->disc_number);

        unsigned char * uniformBytePtr;

        // check if this is a uniform block of repeated bytes
        if((uniformBytePtr = isUniform(buffer, read)) != NULL) {
            // Set the partition table data to 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x?? 
            // where 0x?? is the repeated byte
            memcpy(discInfo->table + (blockNum * 8), &FFs, 4);
            discInfo->table[blockNum * 8 + 7] = *uniformBytePtr;
        } 

        // check if this is a generated junk block
        else if (isSame(buffer, junk, read)) {
            // Write a block of F's to our partition table
            memcpy(discInfo->table + (blockNum * 8), &FFs, 8);
        } 

        // If this is not a junk block or repeated block then it is a data block
        else {
            // Write the block number to the partition table
            // remember, add one for our partition table
            uint64_t tmpBlockNum = blockNum + 1;
            memcpy(discInfo->table + (blockNum * 8), &tmpBlockNum, 8);
        }
        blockNum++;
    }
    fclose(f);

    if (blockNum > 32467) {
        discInfo->isDualLayer = true;
    }

    discInfo->table[7] = discInfo->isGC ? GC_DISC :
        discInfo->isWII && discInfo->isDualLayer ? WII_DL_DISC : WII_DISC;

    return discInfo;
}

void printProfile(struct disc_info * discInfo) {
    if (discInfo->isGC) printf("Gamecube Image Found:\n");
    if (discInfo->isWII) printf("WII Image Found:\n");
    printf("Disc Id: %c%c%c%c%c%c\n", discInfo->id[0], discInfo->id[1], discInfo->id[2], discInfo->id[3], discInfo->id[4], discInfo->id[5]);
    printf("Disc Id: %.*s\n", 6, discInfo->id);
    printf("Disc Number: %d\n", discInfo->disc_number);

    int dataCount = 0;
    int junkCount = 0;
    int uniformCount = 0;
    unsigned char uniformByte = 0;

    uint64_t FFs = 0xFFFFFFFF;
    int blockNum;
    for(blockNum = 1; blockNum < 0x8000; blockNum++) {
        
        unsigned char * uniformBytePtr;

        // if 8 FFs we are a junk block
        if (memcmp(&FFs, discInfo->table + (blockNum * 8), 8) == 0) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformCount = 0;
            }
            junkCount++;
        }
        
        // if 4 FFs we are a repeated byte
        else if (memcmp(&FFs, discInfo->table + (blockNum * 8), 4) == 0) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            uniformBytePtr = discInfo->table + (blockNum * 8) + 7;
            if (uniformCount == 0 || uniformByte == *uniformBytePtr) {
                uniformCount++;    
            } else {
                // if this is a different junk byte dump count and 
                // start counting over with the new junk byte
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformByte = *uniformBytePtr;
                uniformCount = 1;
            }
        } 

        // we are a data block
        else {
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformCount = 0;
            }
            dataCount++;
        }

        if (dataCount > 0){
            printf("%0d blocks of data\n", dataCount);
            dataCount = 0;
        }
        if (junkCount > 0) {
            printf("%0d blocks of junk\n", junkCount);
            junkCount = 0;
        }
        if (uniformCount > 0){
            printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
            uniformCount = 0;
        }
    }
    printf("TOTAL BLOCKS: %d\n", blockNum);
}

/*
 * Profile a disk.  Expects a full iso with valid 
 * disc id and magic number
 */
void profile2(char *file)
{
    FILE *f;

    // if file is null read from stdin
    f = (file != NULL) ? fopen(file, "rb") : stdin;

    unsigned char header[32];
    fread(header, 1, 32, f);
    struct disc_info * discInfo = getDiscInfo(header);
    fclose(f);

    if (discInfo->isGC) printf("Gamecube Image Found:\n");
    if (discInfo->isWII) printf("WII Image Found:\n");
    printf("Disc Id: %c%c%c%c%c%c\n", discInfo->id[0], discInfo->id[1], discInfo->id[2], discInfo->id[3], discInfo->id[4], discInfo->id[5]);
    printf("Disc Id: %.*s\n", 6, discInfo->id);
    printf("Disc Number: %d\n", discInfo->disc_number);
    
    f = (file != NULL) ? fopen(file, "rb") : stdin;
    // Do all of our reading in 0x40000 byte blocks
    unsigned char buffer[0x40000];
    unsigned int i = 0;
    size_t read;

    // keep track of how much data or junk we see
    int dataCount = 0;
    int junkCount = 0;
    int uniformCount = 0;
    unsigned char uniformByte = 0;

    while((read = fread(buffer, 1, 0x40000, f)) > 0) {

        // get the disc info from the first block
        if (discInfo == NULL) {

        }

        // get the junk block for this block number
        unsigned char * junk = getJunkBlock(i++, discInfo->id, discInfo->disc_number);

        unsigned char * uniformBytePtr;
        if((uniformBytePtr = isUniform(buffer, read)) != NULL) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            
            if (uniformCount == 0 || uniformByte == *uniformBytePtr) {
                uniformCount++;    
            } else {
                // if this is a different junk byte dump count and 
                // start counting over with the new junk byte
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformByte = *uniformBytePtr;
                uniformCount = 1;
            }
        } else if (isSame(buffer, junk, read)) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformCount = 0;
            }
            junkCount++;
        } else {
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
                uniformCount = 0;
            }
            dataCount++;
        }
    }
    if (dataCount > 0){
        printf("%0d blocks of data\n", dataCount);
        dataCount = 0;
    }
    if (junkCount > 0) {
        printf("%0d blocks of junk\n", junkCount);
        junkCount = 0;
    }
    if (uniformCount > 0){
        printf("%0d blocks of uniform %02x\n", uniformCount, uniformByte);
        uniformCount = 0;
    }

    fclose(f);
}

int main(int argc, char *argv[])
{
    printf("Hello world\n");
    
    char *inputFile = NULL;
    char *outputFile = NULL;
    bool doProfile = false;

    int opt;
    while ((opt = getopt(argc, argv, "i:o:p")) != -1) {
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
        printProfile(profile(inputFile));
    }
}