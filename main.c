#include <stdio.h>
#include <unistd.h>
#include "hash.h"
#include "partition.h"

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
        handleInputFile(inputFile);
    } else {
        handleInputStream();
    }

    if (outputFile != NULL) {
        FILE *outputF = fopen(outputFile, "wb");
    } else {
        handleOutputStream(0);
    }
}