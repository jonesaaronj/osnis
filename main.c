#include <stdio.h>
#include <unistd.h>
#include "hash.h"
#include "partition.h"

void blah1(char *file)
{
    FILE *f;

    f = fopen(file, "rb");
    unsigned char header[30];
    fread(header, 1, 30, f);
    struct gcm_info * gcmInfo = getGcmInfo(header);
    fclose(f);

    printf("Disc Id: %02x %02x %02x %02x\n", gcmInfo->id[0], gcmInfo->id[1], gcmInfo->id[3], gcmInfo->id[4]);
    printf("Disc Number: %d\n", gcmInfo->disc_number);

    f = fopen(file, "rb"); 
    unsigned char buffer[0x40000];
    unsigned int i = 0;
    size_t read;
    int dataCount = 0;
    int junkCount = 0;
    int uniformCount = 0;
    while((read = fread(buffer, 1, 0x40000, f)) > 0) {

        unsigned char * junk = getJunkBlock(i++, gcmInfo->id, gcmInfo->disc_number);
        unsigned char * uniform = isUniform(buffer, read);

        if(uniform != NULL) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            uniformCount++;
        } else if (same(buffer, junk, 4)) {
            if (dataCount > 0){
                printf("%0d blocks of data\n", dataCount);
                dataCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform\n", uniformCount);
                uniformCount = 0;
            }
            junkCount++;
        } else {
            if (junkCount > 0) {
                printf("%0d blocks of junk\n", junkCount);
                junkCount = 0;
            }
            if (uniformCount > 0){
                printf("%0d blocks of uniform\n", uniformCount);
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
        printf("%0d blocks of uniform\n", uniformCount);
        uniformCount = 0;
    }

    fclose(f);
}

void handleInputStream()
{
    unsigned char buffer[1];

    while(read(0, buffer, sizeof(buffer))>0) {
        printf("%c", buffer[0]);
    }
}

void handleInputFile(char *file)
{
    FILE *f = fopen(file, "rb");
    unsigned char buffer[1];
    while(fread(buffer, 1, 1, f)>0) {
        printf("%c", buffer[0]);
    }
}

void handleOutputFile(FILE *file, unsigned char buffer[], int size)
{
    fwrite(buffer, size, 1, file);
}

void handleOutputStream(unsigned char byte)
{
    
}

int main(int argc, char *argv[])
{
    printf("Hello world\n");
    
    char *inputFile = NULL;
    char *outputFile = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "i:o:")) != -1) {
        switch (opt) {
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

    if (inputFile != NULL) {
        // handleInputFile(inputFile);
        blah1(inputFile);
    } else {
        handleInputStream();
    }

    if (outputFile != NULL) {
        FILE *outputF = fopen(outputFile, "wb");
    } else {
        handleOutputStream(0);
    }
}