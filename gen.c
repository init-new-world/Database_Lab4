#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "extmem.h"

const int BASE_ADDR = 10000;

int SampleTest(){
    Buffer buf; /* A buffer */
    unsigned char *blk; /* A pointer to a block */
    int i = 0;

    /* Initialize the buffer */
    if (!initBuffer(20, 8, &buf))
    {
        perror("Buffer Initialization Failed!\n");
        return -1;
    }

    /* Get a new block in the buffer */
    blk = getNewBlockInBuffer(&buf);

    /* Fill data into the block */
    for (i = 0; i < 8; i++)
        *(blk + i) = 'a' + i;

    /* Write the block to the hard disk */
    if (writeBlockToDisk(blk, 31415926, &buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return -1;
    }

    /* Read the block from the hard disk */
    if ((blk = readBlockFromDisk(31415926, &buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return -1;
    }

    /* Process the data in the block */
    for (i = 0; i < 8; i++)
        printf("%c", *(blk + i));

    printf("\n");
    printf("# of IO's is %d\n", buf.numIO); /* Check the number of IO's */
    return 0;
}

int Rand(int start,int end){
    return rand() % (end - start + 1) + start;
}

void StorageInt(unsigned char* ptr,unsigned int s){
    *ptr = s / (1 << 24);
    *(ptr + 1) = s / (1 << 16) % 256;
    *(ptr + 2) = s / (1 << 8) % 256;
    *(ptr + 3) = s % 256;
}

unsigned int GetInt(unsigned char* ptr){
    unsigned int ret = *ptr * (1 << 24);
    ret += *(ptr + 1) * (1 << 16);
    ret += *(ptr + 2) * (1 << 8);
    return ret + *(ptr + 3);
}

int CreateBlockRandom(){
    Buffer buf;
    unsigned char *blks;
    for(int i = 1;i <= 16;i++){
        if (!initBuffer(999999, 64, &buf))
        {
            perror("Buffer Initialization Failed!\n");
            return -1;
        }
    }

    for(int i = 0;i < 16;++i){
        unsigned int addr = 'R' * BASE_ADDR + i;
        unsigned char *ptr = getNewBlockInBuffer(&buf);
        for(int j = 0; j < 7; ++j) {
            int a = Rand(1,40),b = Rand(1,1000);
            StorageInt(ptr + j*8,a);
            StorageInt(ptr + j*8 + 4,b);
        }
        StorageInt(ptr + 56,0);
        if(i != 15){
            StorageInt(ptr + 60,addr + 1);
        } else {
            StorageInt(ptr + 60,0);
        }
        writeBlockToDisk(ptr, addr, &buf);
    }

    for(int i = 0;i < 16;++i){
        unsigned int addr = 'R' * BASE_ADDR + i;
        unsigned char *ptr = getNewBlockInBuffer(&buf);
        for(int j = 0; j < 7; ++j) {
            int a = Rand(1,40),b = Rand(1,1000);
            StorageInt(ptr + j*8,a);
            StorageInt(ptr + j*8 + 4,b);
        }
        StorageInt(ptr + 56,0);
        if(i != 15){
            StorageInt(ptr + 60,addr + 1);
        } else {
            StorageInt(ptr + 60,0);
        }
        writeBlockToDisk(ptr, addr, &buf);
    }

    for(int i = 0;i < 32;++i){
        unsigned int addr = 'S' * BASE_ADDR + i;
        unsigned char *ptr = getNewBlockInBuffer(&buf);
        for(int j = 0; j < 7; ++j) {
            int a = Rand(20,60),b = Rand(1,1000);
            StorageInt(ptr + j*8,a);
            StorageInt(ptr + j*8 + 4,b);
        }
        StorageInt(ptr + 56,0);
        if(i != 31){
            StorageInt(ptr + 60,addr + 1);
        } else {
            StorageInt(ptr + 60,0);
        }
        writeBlockToDisk(ptr, addr, &buf);
    }
    return 0;
}

int main(){
    srand(time(NULL));
    CreateBlockRandom();
    return 0;
}
