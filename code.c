#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "extmem.h"

const int BASE_ADDR = 10000,OUT_ADDR = 100000,HASH_ADDR = 1000000;

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

struct Relation {
    unsigned int first,second;
};

struct Block {
    unsigned int addr;
    unsigned char *ptr;
    struct Relation rel[7];
    unsigned int nop;
    unsigned int next_block;
};

void ReadRelation(struct Relation *rel,unsigned char *ptr){
    rel -> first = GetInt(ptr);
    rel -> second = GetInt(ptr + 4);
}

void StorageRelation(struct Relation *rel,unsigned char *ptr){
    StorageInt(ptr,rel -> first);
    StorageInt(ptr + 4,rel -> second);
}

int ReadBlock(struct Block *blk,int addr,Buffer *buf){
    blk -> addr = addr;
    unsigned char *ptr = blk -> ptr = readBlockFromDisk(addr,buf);
    if(ptr == NULL)return 0;
    for(int i=0;i<7;++i){
        ReadRelation(&blk->rel[i],ptr + i * 8);
    }
    blk->nop = GetInt(ptr + 56);
    blk->next_block = GetInt(ptr + 60);
    return 1;
}

int CmpFirst(const void *a,const void *b){
    unsigned int ax = ((struct Relation *)a)->first,bx = ((struct Relation *)b)->first;
    return ax-bx;
}

int CmpSecond(const void *a,const void *b){
    unsigned int ax = ((struct Relation *)a)->second,bx = ((struct Relation *)b)->second;
    return ax-bx;
}

void RefreshBlock(struct Block *blk){
    unsigned char *ptr = blk -> ptr;
    for(int i=0;i<7;i++){
        StorageRelation(blk -> rel + i,ptr + i * 8);
    }
    StorageInt(ptr + 56,blk->nop);
    StorageInt(ptr + 60,blk->next_block);
}

void PrintBlock(struct Block *blk){
    printf("[Block Addr#%d]\n",blk->addr);
    for(int i=0;i<7;i++){
        printf("#%d: first = %d,second = %d\n",i,blk->rel[i].first,blk->rel[i].second);
    }
    printf("nop = %d\n",blk->nop);
    printf("next_block = %d\n",blk->next_block);
    printf("[Block end]\n");
}

void BlockSort(struct Block *blk,int is_first,Buffer *buf){
    if(is_first){
        qsort(blk->rel,7,sizeof(struct Relation),CmpFirst);
        RefreshBlock(blk);
        writeBlockToDisk(blk->ptr,blk->addr,buf);
        freeBlockInBuffer(blk->ptr,buf);
    } else {
        qsort(blk->rel,7,sizeof(struct Relation),CmpSecond);
        RefreshBlock(blk);
        writeBlockToDisk(blk->ptr,blk->addr,buf);
        freeBlockInBuffer(blk->ptr,buf);
    }
}

void AllBlockSort(){
    Buffer buf;
    initBuffer(1000000, 64, &buf);
    for(int i=0;i<16;i++){
        unsigned int addr = BASE_ADDR * 'R' + i;
        struct Block now;
        ReadBlock(&now,addr,&buf);
        //PrintBlock(&now);
        BlockSort(&now,1,&buf);
        //PrintBlock(&now);
    }
    for(int i=0;i<32;i++){
        unsigned int addr = BASE_ADDR * 'S' + i;
        struct Block now;
        ReadBlock(&now,addr,&buf);
        //PrintBlock(&now);
        BlockSort(&now,1,&buf);
        //PrintBlock(&now);
    }
}

void SortXBlock(int first_addr,int last_addr,int is_first,int mx_addr){
    Buffer buf;
    initBuffer(520,64,&buf);
    struct Block bk_list[8];
    for(int i=0;i<7 && first_addr+i<last_addr;i++){
        ReadBlock(&bk_list[i],first_addr+i,&buf);
    }
    int out_block=0,out_rel=0,reptr[7]={0};
    bk_list[7].ptr = getNewBlockInBuffer(&buf);
    for(int i=0;i<7*(last_addr - first_addr);++i){
        unsigned int mn=9999;
        int mnx=-1;
        for (int j = 0; j < last_addr - first_addr; ++j) {
            if(is_first){
                if(reptr[j] < 7 ){
                    if(bk_list[j].rel[reptr[j]].first<mn){mn=bk_list[j].rel[reptr[j]].first;mnx=j;}
                } else continue;
            } else {
                if(reptr[j] < 7 ){
                    if(bk_list[j].rel[reptr[j]].second<mn){mn=bk_list[j].rel[reptr[j]].second;mnx=j;}
                } else continue;
            }
        }
        bk_list[7].rel[out_rel].first = bk_list[mnx].rel[reptr[mnx]].first;
        bk_list[7].rel[out_rel].second = bk_list[mnx].rel[reptr[mnx]].second;
        reptr[mnx]++;
        out_rel++;
        if(out_rel == 7){
            //puts("***********************");
            out_rel = 0;
            bk_list[7].nop = 0;
            bk_list[7].addr = first_addr + out_block;
            if(bk_list[7].addr + 1 == mx_addr)bk_list[7].next_block = 0;
            else bk_list[7].next_block = first_addr + out_block + 1;
            RefreshBlock(&bk_list[7]);
            //PrintBlock(&bk_list[7]);
            out_block++;
            writeBlockToDisk(bk_list[7].ptr,bk_list[7].addr,&buf);
        }
    }
}

void AllSortR(int is_first){
    SortXBlock('R'*BASE_ADDR,'R'*BASE_ADDR+7,1,'R'*BASE_ADDR+16);
    SortXBlock('R'*BASE_ADDR+7,'R'*BASE_ADDR+14,1,'R'*BASE_ADDR+16);
    SortXBlock('R'*BASE_ADDR+14,'R'*BASE_ADDR+16,1,'R'*BASE_ADDR+16);

    Buffer buf;
    initBuffer(520,64,&buf);

    struct Block bk_list[4];
    unsigned int bk_ptr[3]={0},addr='R'*BASE_ADDR+16,flag[3]={0},out_rel=0,out_block=0;
    unsigned int mx_addr = 'R'*BASE_ADDR+16,out;
    ReadBlock(&bk_list[0],'R'*BASE_ADDR,&buf);
    ReadBlock(&bk_list[1],'R'*BASE_ADDR+7,&buf);
    ReadBlock(&bk_list[2],'R'*BASE_ADDR+14,&buf);
    //PrintBlock(&bk_list[0]);
    //PrintBlock(&bk_list[1]);
    //PrintBlock(&bk_list[2]);
    bk_list[3].ptr = getNewBlockInBuffer(&buf);
    for(int i=0;i<7*16;i++){
        unsigned int mn=9999;
        int mnx=-1;
        for(int j=0;j<3;j++){
            if(bk_ptr[j] == 7){
                flag[j] = 1;
                if(j == 0 && bk_list[0].next_block != 'R'*BASE_ADDR+7){
                    freeBlockInBuffer(bk_list[0].ptr,&buf);
                    ReadBlock(&bk_list[0],bk_list[0].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 1 && bk_list[1].next_block != 'R'*BASE_ADDR+14){
                    freeBlockInBuffer(bk_list[1].ptr,&buf);
                    ReadBlock(&bk_list[1],bk_list[1].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 2 && bk_list[2].next_block != 0){
                    freeBlockInBuffer(bk_list[2].ptr,&buf);
                    ReadBlock(&bk_list[2],bk_list[2].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
            }
            if(!flag[j]){
                if(is_first){
                    if(bk_list[j].rel[bk_ptr[j]].first<mn){
                        mn = bk_list[j].rel[bk_ptr[j]].first;
                        mnx = j;
                    }
                } else {
                    if(bk_list[j].rel[bk_ptr[j]].second<mn){
                        mn = bk_list[j].rel[bk_ptr[j]].second;
                        mnx = j;
                    }
                }
            }
        }
        bk_list[3].rel[out_rel].first = bk_list[mnx].rel[bk_ptr[mnx]].first;
        bk_list[3].rel[out_rel].second = bk_list[mnx].rel[bk_ptr[mnx]].second;
        //printf("[DEBUG]MNX = %d,BK_PTR = %d,F = %d,S = %d\n",mnx,bk_ptr[mnx],bk_list[mnx].rel[bk_ptr[mnx]].first,bk_list[mnx].rel[bk_ptr[mnx]].second);
        bk_ptr[mnx]++;
        out_rel++;
        if(out_rel == 7){
            out_rel = 0;
            bk_list[3].nop = 0;
            bk_list[3].addr = mx_addr + out_block;
            if(out_block == 15)bk_list[3].next_block = 0;
            else bk_list[3].next_block = mx_addr + out_block + 1;
            RefreshBlock(&bk_list[3]);
            //PrintBlock(&bk_list[3]);
            out_block++;
            writeBlockToDisk(bk_list[3].ptr,bk_list[3].addr,&buf);
        }
    }
    freeBlockInBuffer(bk_list[0].ptr,&buf);
    freeBlockInBuffer(bk_list[1].ptr,&buf);
    freeBlockInBuffer(bk_list[2].ptr,&buf);
    for(int i=0;i<16;i++){
        ReadBlock(&bk_list[3],'R'*BASE_ADDR+i+16,&buf);
        bk_list[3].next_block = 'R'*BASE_ADDR+i+1;
        bk_list[3].addr = 'R'*BASE_ADDR+i;
        if('R'*BASE_ADDR+i+1 == mx_addr)bk_list[3].next_block = 0;
        RefreshBlock(&bk_list[3]);
        //PrintBlock(&bk_list[3]);
        writeBlockToDisk(bk_list[3].ptr,'R'*BASE_ADDR+i,&buf);
        dropBlockOnDisk('R'*BASE_ADDR+i+16);
    }
}

void AllSortS(int is_first){
    SortXBlock('S'*BASE_ADDR,'S'*BASE_ADDR+7,1,'S'*BASE_ADDR+32);
    SortXBlock('S'*BASE_ADDR+7,'S'*BASE_ADDR+14,1,'S'*BASE_ADDR+32);
    SortXBlock('S'*BASE_ADDR+14,'S'*BASE_ADDR+21,1,'S'*BASE_ADDR+32);
    SortXBlock('S'*BASE_ADDR+21,'S'*BASE_ADDR+28,1,'S'*BASE_ADDR+32);
    SortXBlock('S'*BASE_ADDR+28,'S'*BASE_ADDR+32,1,'S'*BASE_ADDR+32);

    Buffer buf;
    initBuffer(520,64,&buf);

    struct Block bk_list[6];
    unsigned int bk_ptr[5]={0},addr='S'*BASE_ADDR+32,flag[5]={0},out_rel=0,out_block=0;
    unsigned int mx_addr = 'S'*BASE_ADDR+32,out;
    ReadBlock(&bk_list[0],'S'*BASE_ADDR,&buf);
    ReadBlock(&bk_list[1],'S'*BASE_ADDR+7,&buf);
    ReadBlock(&bk_list[2],'S'*BASE_ADDR+14,&buf);
    ReadBlock(&bk_list[3],'S'*BASE_ADDR+21,&buf);
    ReadBlock(&bk_list[4],'S'*BASE_ADDR+28,&buf);
    //PrintBlock(&bk_list[0]);
    //PrintBlock(&bk_list[1]);
    //PrintBlock(&bk_list[2]);
    //PrintBlock(&bk_list[3]);
    //PrintBlock(&bk_list[4]);
    bk_list[5].ptr = getNewBlockInBuffer(&buf);
    for(int i=0;i<7*32;i++){
        unsigned int mn=9999;
        int mnx=-1;
        for(int j=0;j<5;j++){
            if(bk_ptr[j] == 7){
                flag[j] = 1;
                if(j == 0 && bk_list[0].next_block != 'S'*BASE_ADDR+7){
                    freeBlockInBuffer(bk_list[0].ptr,&buf);
                    ReadBlock(&bk_list[0],bk_list[0].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 1 && bk_list[1].next_block != 'S'*BASE_ADDR+14){
                    freeBlockInBuffer(bk_list[1].ptr,&buf);
                    ReadBlock(&bk_list[1],bk_list[1].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 2 && bk_list[2].next_block != 'S'*BASE_ADDR+21){
                    freeBlockInBuffer(bk_list[2].ptr,&buf);
                    ReadBlock(&bk_list[2],bk_list[2].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 3 && bk_list[3].next_block != 'S'*BASE_ADDR+28){
                    freeBlockInBuffer(bk_list[3].ptr,&buf);
                    ReadBlock(&bk_list[3],bk_list[3].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
                if(j == 4 && bk_list[4].next_block != 0){
                    freeBlockInBuffer(bk_list[4].ptr,&buf);
                    ReadBlock(&bk_list[4],bk_list[4].next_block,&buf);
                    flag[j] = 0;
                    bk_ptr[j] = 0;
                }
            }
            if(!flag[j]){
                //printf("[DEBUG] %d = %d\n",bk_list[j].rel[bk_ptr[j]].first,j);
                if(is_first){
                    if(bk_list[j].rel[bk_ptr[j]].first<mn){
                        mn = bk_list[j].rel[bk_ptr[j]].first;
                        mnx = j;
                    }
                } else {
                    if(bk_list[j].rel[bk_ptr[j]].second<mn){
                        mn = bk_list[j].rel[bk_ptr[j]].second;
                        mnx = j;
                    }
                }
            }
        }
        bk_list[5].rel[out_rel].first = bk_list[mnx].rel[bk_ptr[mnx]].first;
        bk_list[5].rel[out_rel].second = bk_list[mnx].rel[bk_ptr[mnx]].second;
        //printf("[DEBUG]MNX = %d,BK_PTR = %d,F = %d,S = %d\n",mnx,bk_ptr[mnx],bk_list[mnx].rel[bk_ptr[mnx]].first,bk_list[mnx].rel[bk_ptr[mnx]].second);
        bk_ptr[mnx]++;
        out_rel++;
        if(out_rel == 7){
            out_rel = 0;
            bk_list[5].nop = 0;
            bk_list[5].addr = mx_addr + out_block;
            if(out_block == 31)bk_list[5].next_block = 0;
            else bk_list[5].next_block = mx_addr + out_block + 1;
            RefreshBlock(&bk_list[5]);
            //PrintBlock(&bk_list[5]);
            out_block++;
            writeBlockToDisk(bk_list[5].ptr,bk_list[5].addr,&buf);
        }
    }
    freeBlockInBuffer(bk_list[0].ptr,&buf);
    freeBlockInBuffer(bk_list[1].ptr,&buf);
    freeBlockInBuffer(bk_list[2].ptr,&buf);
    freeBlockInBuffer(bk_list[3].ptr,&buf);
    freeBlockInBuffer(bk_list[4].ptr,&buf);
    for(int i=0;i<32;i++){
        ReadBlock(&bk_list[5],'S'*BASE_ADDR+i+32,&buf);
        bk_list[5].next_block = 'S'*BASE_ADDR+i+1;
        bk_list[5].addr = 'S'*BASE_ADDR+i;
        if('S'*BASE_ADDR+i+1 == mx_addr)bk_list[5].next_block = 0;
        RefreshBlock(&bk_list[5]);
        //PrintBlock(&bk_list[5]);
        writeBlockToDisk(bk_list[5].ptr,'S'*BASE_ADDR+i,&buf);
        dropBlockOnDisk('S'*BASE_ADDR+i+32);
    }
}

void LinearSelect(char rel_name,int is_first,int number,Buffer *buf){
    unsigned int addr = rel_name * BASE_ADDR,end_addr,out_count = 0,out_addr = rel_name * OUT_ADDR,reflag = 0;
    struct Block blk[2];
    if(rel_name == 'R'){
        end_addr = addr + 16;
    } else {
        end_addr = addr + 32;
    }
    blk[1].ptr = getNewBlockInBuffer(buf);
    for(unsigned int i = addr;i < end_addr;i++){
        ReadBlock(&blk[0],i,buf);
        //printf("[DEBUG] %d buf_num_blk %d\n",i,buf->numAllBlk);
        for(int j = 0;j < 7;j++){
            if(is_first){
                //printf("[DEBUG] blk0.%d = %d,blk0.%d = %d\n",j,blk[0].rel[j].first,j,blk[0].rel[j].second);
                if(blk[0].rel[j].first == number){
                    blk[1].rel[out_count].first = blk[0].rel[j].first;
                    blk[1].rel[out_count].second = blk[0].rel[j].second;
                    out_count++;
                    reflag = 1;
                    if(out_count == 7){
                        blk[1].addr = out_addr;
                        blk[1].nop = 7;
                        blk[1].next_block = out_addr + 1;
                        RefreshBlock(&blk[1]);
                        //PrintBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        out_addr++;
                    }
                }
            } else {
                if(blk[0].rel[j].second == number){
                    blk[1].rel[out_count].first = blk[0].rel[j].first;
                    blk[1].rel[out_count].second = blk[0].rel[j].second;
                    out_count++;
                    reflag = 1;
                    if(out_count == 7){
                        blk[1].addr = out_addr;
                        blk[1].nop = 7;
                        blk[1].next_block = out_addr + 1;
                        RefreshBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        out_addr++;
                    }
                }
            }
        }
        freeBlockInBuffer(blk[0].ptr,buf);
    }
    if(out_count > 0){
        blk[1].addr = out_addr;
        blk[1].nop = out_count;
        blk[1].next_block = 0;
        RefreshBlock(&blk[1]);
        //PrintBlock(&blk[1]);
        out_count = 0;
        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        out_addr++;
    } else {
        if(reflag){
            blk[1].addr = out_addr - 1;
            blk[1].nop = 7;
            blk[1].next_block = 0;
            RefreshBlock(&blk[1]);
            //PrintBlock(&blk[1]);
            out_count = 0;
            writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        } else {
            blk[1].addr = out_addr;
            blk[1].nop = 0;
            blk[1].next_block = 0;
            RefreshBlock(&blk[1]);
            //PrintBlock(&blk[1]);
            out_count = 0;
            writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        }
    }
    freeBlockInBuffer(blk[1].ptr,buf);
}

void BinarySelect(char rel_name,int is_first,int number,Buffer *buf){
    unsigned int addr = rel_name * BASE_ADDR,end_addr,out_count = 0,out_addr = rel_name * OUT_ADDR,reflag = 0;
    struct Block blk[2];
    if(rel_name == 'R'){
        end_addr = addr + 16;
    } else {
        end_addr = addr + 32;
    }
    unsigned int l = addr,r = end_addr - 1,ans = -1;
    while(l <= r){
        unsigned int mid = (l + r) / 2;
        ReadBlock(&blk[0],mid,buf);
        if(is_first){
            if(blk[0].rel[0].first>number)r = mid - 1;
            else if(blk[0].rel[6].first<number) l = mid + 1;
            else if(blk[0].rel[0].first<number && number<blk[0].rel[6].first){
                ans = mid;
                freeBlockInBuffer(blk[0].ptr,buf);
                break;
            }
            else if(blk[0].rel[0].first<number && number==blk[0].rel[6].first){
                ans = mid;
                freeBlockInBuffer(blk[0].ptr,buf);
                break;
            }
            else if(blk[0].rel[0].first==number){
                r = mid - 1;
                ans = mid;
            }
            freeBlockInBuffer(blk[0].ptr,buf);
        } else {
            if(blk[0].rel[0].second>number)r = mid - 1;
            else if(blk[0].rel[6].second<number) l = mid + 1;
            else if(blk[0].rel[0].second<number && number<blk[0].rel[6].second){
                ans = mid;
                freeBlockInBuffer(blk[0].ptr,buf);
                break;
            }
            else if(blk[0].rel[0].second<number && number==blk[0].rel[6].second){
                ans = mid;
                freeBlockInBuffer(blk[0].ptr,buf);
                break;
            }
            else if(blk[0].rel[0].second==number){
                r = mid - 1;
                ans = mid;
            }
            freeBlockInBuffer(blk[0].ptr,buf);
        }
    }
    blk[1].ptr = getNewBlockInBuffer(buf);
    if(ans == -1){
        blk[1].addr = out_addr;
        blk[1].nop = 0;
        blk[1].next_block = 0;
        RefreshBlock(&blk[1]);
        //PrintBlock(&blk[1]);
        out_count = 0;
        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        freeBlockInBuffer(blk[1].ptr,buf);
        return;
    }
    for(unsigned int i = ans;i < end_addr;i++){
        ReadBlock(&blk[0],i,buf);
        //printf("[DEBUG] %d buf_num_blk %d\n",i,buf->numAllBlk);
        for(int j = 0;j < 7;j++){
            if(is_first){
                //printf("[DEBUG] blk0.%d = %d,blk0.%d = %d\n",j,blk[0].rel[j].first,j,blk[0].rel[j].second);
                if(blk[0].rel[j].first == number){
                    blk[1].rel[out_count].first = blk[0].rel[j].first;
                    blk[1].rel[out_count].second = blk[0].rel[j].second;
                    out_count++;
                    reflag = 1;
                    if(out_count == 7){
                        blk[1].addr = out_addr;
                        blk[1].nop = 7;
                        blk[1].next_block = out_addr + 1;
                        RefreshBlock(&blk[1]);
                        //PrintBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        out_addr++;
                    }
                } else {
                    if(i!=ans && !reflag){
                        blk[1].addr = out_addr;
                        blk[1].nop = 0;
                        blk[1].next_block = 0;
                        RefreshBlock(&blk[1]);
                        //PrintBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        freeBlockInBuffer(blk[1].ptr,buf);
                        return;
                    }
                    if(reflag){
                        reflag=2;
                        break;
                    }
                }
            } else {
                if(blk[0].rel[j].second == number){
                    blk[1].rel[out_count].first = blk[0].rel[j].first;
                    blk[1].rel[out_count].second = blk[0].rel[j].second;
                    out_count++;
                    reflag = 1;
                    if(out_count == 7){
                        blk[1].addr = out_addr;
                        blk[1].nop = 7;
                        blk[1].next_block = out_addr + 1;
                        RefreshBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        out_addr++;
                    }
                } else {
                    if(i!=ans && !reflag){
                        blk[1].addr = out_addr;
                        blk[1].nop = 0;
                        blk[1].next_block = 0;
                        RefreshBlock(&blk[1]);
                        //PrintBlock(&blk[1]);
                        out_count = 0;
                        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
                        freeBlockInBuffer(blk[1].ptr,buf);
                        return;
                    }
                    if(reflag){
                        reflag=2;
                        break;
                    }
                }
            }
        }
        freeBlockInBuffer(blk[0].ptr,buf);
        if(reflag == 2)break;
    }
    if(out_count > 0){
        blk[1].addr = out_addr;
        blk[1].nop = out_count;
        blk[1].next_block = 0;
        RefreshBlock(&blk[1]);
        PrintBlock(&blk[1]);
        out_count = 0;
        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        out_addr++;
    } else {
        if(reflag){
            blk[1].addr = out_addr - 1;
            blk[1].nop = 7;
            blk[1].next_block = 0;
            RefreshBlock(&blk[1]);
            //PrintBlock(&blk[1]);
            out_count = 0;
            writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        } else {
            blk[1].addr = out_addr;
            blk[1].nop = 0;
            blk[1].next_block = 0;
            RefreshBlock(&blk[1]);
            //PrintBlock(&blk[1]);
            out_count = 0;
            writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
        }
    }
    freeBlockInBuffer(blk[1].ptr,buf);
}

void MakeHashBlock(int is_first){
    Buffer buf;
    initBuffer(520, 64, &buf);
    struct Block bk_list[2];
    bk_list[1].ptr = getNewBlockInBuffer(&buf);
    unsigned int hash_addr = 'R' * HASH_ADDR,main_addr = 'R' * BASE_ADDR,out_block=0,out_rel=0,last_rel=9999;
    for(int i=0;i<16;i++){
        ReadBlock(&bk_list[0],main_addr+i,&buf);
        //PrintBlock(&bk_list[0]);
        for(int j=0;j<7;j++){
            if(is_first){
                if(bk_list[0].rel[j].first == last_rel){
                    bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                    bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                    out_rel++;
                    if(out_rel == 7){
                        out_rel = 0;
                        bk_list[1].nop = 7;
                        bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
                        bk_list[1].next_block = bk_list[1].addr + 1;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        out_block++;
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                    }
                } else {
                    if(last_rel != 9999){
                        //printf("[DEBUG] last_rel = %d\n",last_rel);
                        if(out_rel != 0){
                            bk_list[1].nop = out_rel;
                            bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
                        }
                        bk_list[1].next_block = 0;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                        last_rel = bk_list[0].rel[j].first;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    } else {
                        last_rel = bk_list[0].rel[j].first;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    }
                    out_block = 0;
                }
            } else {
                if(bk_list[0].rel[j].second == last_rel){
                    bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                    bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                    out_rel++;
                    if(out_rel == 7){
                        out_rel = 0;
                        bk_list[1].nop = 7;
                        bk_list[1].addr = hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
                        bk_list[1].next_block = bk_list[1].addr + 1;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        out_block++;
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                    }
                } else {
                    if(last_rel != 9999){
                        if(out_rel != 0){
                            bk_list[1].nop = out_rel;
                            bk_list[1].addr = hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
                        }
                        bk_list[1].next_block = 0;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                        last_rel = bk_list[0].rel[j].second;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    } else {
                        last_rel = bk_list[0].rel[j].second;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    }
                    out_block = 0;
                }
            }
        }
        freeBlockInBuffer(bk_list[0].ptr,&buf);
    }
    if(out_rel != 0){
        bk_list[1].nop = out_rel;
        if(is_first)bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
        else hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
        bk_list[1].next_block = 0;
        RefreshBlock(&bk_list[1]);
        //PrintBlock(&bk_list[1]);
        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
    }

    hash_addr = 'S' * HASH_ADDR,main_addr = 'S' * BASE_ADDR,out_block=0,out_rel=0,last_rel=9999;
    for(int i=0;i<32;i++){
        ReadBlock(&bk_list[0],main_addr+i,&buf);
        //PrintBlock(&bk_list[0]);
        for(int j=0;j<7;j++){
            if(is_first){
                if(bk_list[0].rel[j].first == last_rel){
                    bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                    bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                    out_rel++;
                    if(out_rel == 7){
                        out_rel = 0;
                        bk_list[1].nop = 7;
                        bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
                        bk_list[1].next_block = bk_list[1].addr + 1;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        out_block++;
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                    }
                } else {
                    if(last_rel != 9999){
                        //printf("[DEBUG] last_rel = %d\n",last_rel);
                        if(out_rel != 0){
                            bk_list[1].nop = out_rel;
                            bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
                        }
                        bk_list[1].next_block = 0;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                        last_rel = bk_list[0].rel[j].first;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    } else {
                        last_rel = bk_list[0].rel[j].first;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    }
                    out_block = 0;
                }
            } else {
                if(bk_list[0].rel[j].second == last_rel){
                    bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                    bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                    out_rel++;
                    if(out_rel == 7){
                        out_rel = 0;
                        bk_list[1].nop = 7;
                        bk_list[1].addr = hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
                        bk_list[1].next_block = bk_list[1].addr + 1;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        out_block++;
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                    }
                } else {
                    if(last_rel != 9999){
                        if(out_rel != 0){
                            bk_list[1].nop = out_rel;
                            bk_list[1].addr = hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
                        }
                        bk_list[1].next_block = 0;
                        RefreshBlock(&bk_list[1]);
                        //PrintBlock(&bk_list[1]);
                        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
                        last_rel = bk_list[0].rel[j].second;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    } else {
                        last_rel = bk_list[0].rel[j].second;
                        out_rel = 0;
                        bk_list[1].rel[out_rel].first = bk_list[0].rel[j].first;
                        bk_list[1].rel[out_rel].second = bk_list[0].rel[j].second;
                        out_rel++;
                    }
                    out_block = 0;
                }
            }
        }
        freeBlockInBuffer(bk_list[0].ptr,&buf);
    }
    if(out_rel != 0){
        bk_list[1].nop = out_rel;
        if(is_first)bk_list[1].addr = hash_addr + bk_list[1].rel[0].first * 1000 + out_block;
        else hash_addr + bk_list[1].rel[0].second * 1000 + out_block;
        bk_list[1].next_block = 0;
        RefreshBlock(&bk_list[1]);
        //PrintBlock(&bk_list[1]);
        writeBlockToDisk(bk_list[1].ptr,bk_list[1].addr,&buf);
    }

    freeBlockInBuffer(bk_list[1].ptr,&buf);
}

void HashSearch(char rel_name,int number,Buffer *buf){
    unsigned int addr = rel_name * HASH_ADDR + number * 1000,end_addr,out_count = 0,out_addr = rel_name * OUT_ADDR,reflag = 0;
    struct Block blk[2];
    if(rel_name == 'R'){
        end_addr = addr + 16;
    } else {
        end_addr = addr + 32;
    }
    //printf("[DEBUG] addr = %d\n",addr);
    if(!ReadBlock(&blk[0],addr,buf)){
        printf("Cannot find it.");
        return;
    }
    blk[1].ptr = getNewBlockInBuffer(buf);
    int out_rel = 0,out_block = 0;
    do {
        for(unsigned int i=0;i<blk[0].nop;i++){
            blk[1].rel[out_rel].first = blk[0].rel[i].first;
            blk[1].rel[out_rel].second = blk[0].rel[i].second;
            out_rel++;
            if(out_rel == 7){
                out_rel = 0;
                blk[1].nop = 7;
                blk[1].addr = out_addr + out_block;
                blk[1].next_block = blk[1].addr + 1;
                RefreshBlock(&blk[1]);
                //PrintBlock(&blk[1]);
                out_block++;
                writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
            }
        }
        addr = blk[0].next_block;
        if(addr!=0)ReadBlock(&blk[0],addr,buf);
    } while(addr != 0);
    if(out_rel != 0){
        blk[1].nop = out_rel;
        blk[1].addr = out_addr + out_block;
        blk[1].next_block = 0;
        RefreshBlock(&blk[1]);
        //PrintBlock(&blk[1]);
        writeBlockToDisk(blk[1].ptr,blk[1].addr,buf);
    }
    freeBlockInBuffer(blk[0].ptr,buf);
    freeBlockInBuffer(blk[1].ptr,buf);
}

void PrintBlockPtr(struct Block *blk){
    printf("[BLOCK PTR]");
    for(int i=0;i<14;i++){
        printf("[Addr+%d] value = %d\n",i*4, GetInt(blk->ptr+i*4));
    }
    printf("[Addr+%d] nop = %d\n",56, GetInt(blk->ptr+56));
    printf("[Addr+%d] next_block = %d\n",60, GetInt(blk->ptr+60));
}

void Projection(char rel_name,int is_first,Buffer *buf){
    unsigned int addr = rel_name * BASE_ADDR,end_addr,out_count = 0,out_addr = rel_name * OUT_ADDR,reflag = 0,out_block = 0;
    if(rel_name == 'R'){
        end_addr = addr + 16;
    } else {
        end_addr = addr + 32;
    }
    struct Block blk[2];
    blk[1].ptr= getNewBlockInBuffer(buf);
    int pointer = 0;
    for(unsigned int i=addr;i<end_addr;i++){
        ReadBlock(&blk[0],i,buf);
        //PrintBlock(&blk[0]);
        for(int j=0;j<7;j++){
            if(is_first)StorageInt(blk[1].ptr+pointer * 4,blk[0].rel[j].first);
            else StorageInt(blk[1].ptr+pointer * 4,blk[0].rel[j].second);
            pointer++;
            if(pointer == 14){
                StorageInt(blk[1].ptr+56,14);
                StorageInt(blk[1].ptr+60,out_addr + out_block + 1);
                writeBlockToDisk(blk[1].ptr,out_addr + out_block,buf);
                //PrintBlockPtr(&blk[1]);
                out_block++;
                pointer = 0;
            }
        }
        freeBlockInBuffer(blk[0].ptr,buf);
    }
    if(pointer != 0){
        StorageInt(blk[1].ptr+56,14);
        StorageInt(blk[1].ptr+60,0);
        writeBlockToDisk(blk[1].ptr,out_addr + out_block,buf);
        //PrintBlockPtr(&blk[1]);
    } else {
        StorageInt(blk[1].ptr+56,pointer);
        StorageInt(blk[1].ptr+60,0);
        writeBlockToDisk(blk[1].ptr,out_addr + out_block,buf);
        //PrintBlockPtr(&blk[1]);
    }
    freeBlockInBuffer(blk[1].ptr,buf);
}

void NestLoopJoin(Buffer *buf){
    unsigned int Raddr = 'R' * BASE_ADDR,Rend_addr = Raddr + 16;
    unsigned int Saddr = 'S' * BASE_ADDR,Send_addr = Saddr + 32;
    unsigned int out_count = 0,out_addr = 'O' * OUT_ADDR,reflag = 0,out_block = 0;
    struct Block blk[3];
    blk[2].ptr = getNewBlockInBuffer(buf);
    for(unsigned int i=Raddr;i<Rend_addr;i++){
        ReadBlock(&blk[0],i,buf);
        //PrintBlock(&blk[0]);
        //puts("UNREAL");
        for(unsigned int j=Saddr;j<Send_addr;j++){
            ReadBlock(&blk[1],j,buf);
            //PrintBlock(&blk[1]);
            for(unsigned int k=0;k<7;k++){
                for(unsigned int l=0;l<7;l++){
                    if(blk[0].rel[k].first == blk[1].rel[l].first){
                        blk[2].rel[out_count].first = blk[0].rel[k].first;
                        blk[2].rel[out_count].second = blk[0].rel[k].second;
                        blk[2].rel[out_count+1].first = blk[1].rel[l].first;
                        blk[2].rel[out_count+1].second = blk[1].rel[l].second;
                        out_count += 2;
                        if(out_count == 6){
                            out_count = 0;
                            blk[2].nop = 3;
                            blk[2].addr = out_addr+out_block;
                            blk[2].next_block = blk[2].addr + 1;
                            RefreshBlock(&blk[2]);
                            //PrintBlock(&blk[2]);
                            writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
                            out_block++;
                        }
                    }
                }
            }
            freeBlockInBuffer(blk[1].ptr,buf);
        }
        freeBlockInBuffer(blk[0].ptr,buf);
    }
    if(out_count != 0){
        blk[2].nop = out_count / 2;
        blk[2].addr = out_addr + out_block;
        blk[2].next_block = 0;
        RefreshBlock(&blk[2]);
        //PrintBlock(&blk[2]);
        writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
    } else if(out_block != 0) {
        blk[2].next_block = 0;
        writeBlockToDisk(blk[2].ptr, out_addr + out_block - 1, buf);
    }
    freeBlockInBuffer(blk[2].ptr,buf);
}

void SortMergeJoin(Buffer *buf){
    unsigned int Raddr = 'R' * BASE_ADDR,Rend_addr = Raddr + 16;
    unsigned int Saddr = 'S' * BASE_ADDR,Send_addr = Saddr + 32;
    unsigned int lptr = Raddr,rptr = Saddr,lin = 0,rin = 0;
    unsigned int out_count = 0,out_addr = 'O' * OUT_ADDR,reflag = 0,out_block = 0;
    struct Block blk[3];
    ReadBlock(&blk[0],Raddr,buf);
    ReadBlock(&blk[1],Saddr,buf);
    blk[2].ptr = getNewBlockInBuffer(buf);
    while(lptr<Rend_addr && rptr<Send_addr){
        //printf("[DEBUG] Lin0.f = %d,Lin1.f = %d\n",blk[0].rel[lin].first,blk[1].rel[rin].first);
        if(blk[0].rel[lin].first == blk[1].rel[rin].first){
            blk[2].rel[out_count].first = blk[0].rel[lin].first;
            blk[2].rel[out_count].second = blk[0].rel[lin].second;
            blk[2].rel[out_count+1].first = blk[1].rel[rin].first;
            blk[2].rel[out_count+1].second = blk[1].rel[rin].second;
            out_count += 2;
            if(out_count == 6){
                out_count = 0;
                blk[2].nop = 3;
                blk[2].addr = out_addr+out_block;
                blk[2].next_block = blk[2].addr + 1;
                RefreshBlock(&blk[2]);
                //PrintBlock(&blk[2]);
                writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
                out_block++;
            }
            lin++;
            rin++;
        } else {
            if(blk[0].rel[lin].first < blk[1].rel[rin].first){
                lin++;
            } else {
                rin++;
            }
        }
        if(lin == 7){
            lin = 0;
            lptr++;
            freeBlockInBuffer(blk[0].ptr,buf);
            if(lptr<Rend_addr){
                ReadBlock(&blk[0],lptr,buf);
            }
        }
        if(rin == 7){
            rin = 0;
            rptr++;
            freeBlockInBuffer(blk[1].ptr,buf);
            if(rptr<Send_addr){
                ReadBlock(&blk[1],rptr,buf);
                //PrintBlock(&blk[1]);
            }
        }
    }
    if(out_count != 0){
        blk[2].nop = out_count / 2;
        blk[2].addr = out_addr + out_block;
        blk[2].next_block = 0;
        RefreshBlock(&blk[2]);
        //PrintBlock(&blk[2]);
        writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
    } else if(out_block != 0) {
        blk[2].next_block = 0;
        writeBlockToDisk(blk[2].ptr, out_addr + out_block - 1, buf);
    }
    freeBlockInBuffer(blk[2].ptr,buf);
}

void HashJoin(Buffer *buf){
    unsigned int Raddr = 'R' * HASH_ADDR;
    unsigned int Saddr = 'S' * HASH_ADDR;
    unsigned int out_count = 0,out_addr = 'O' * OUT_ADDR,out_block = 0;
    struct Block blk[3];
    blk[2].ptr = getNewBlockInBuffer(buf);
    for(unsigned int i=20;i<=40;i++){
        unsigned int Rreal = Raddr + i * 1000;
        unsigned int Sreal = Saddr + i * 1000;
        //printf("%d %d\n",Rreal,Sreal);
        if(ReadBlock(&blk[0],Rreal,buf) && ReadBlock(&blk[1],Sreal,buf)){
            //puts("Sreal");
            for(unsigned int j=0;;j++){
                for(unsigned int k=0;;k++){
                    blk[2].rel[out_count].first = blk[0].rel[j].first;
                    blk[2].rel[out_count].second = blk[0].rel[j].second;
                    blk[2].rel[out_count+1].first = blk[1].rel[k].first;
                    blk[2].rel[out_count+1].second = blk[1].rel[k].second;
                    out_count += 2;
                    if(out_count == 6){
                        out_count = 0;
                        blk[2].nop = 3;
                        blk[2].addr = out_addr+out_block;
                        blk[2].next_block = blk[2].addr + 1;
                        RefreshBlock(&blk[2]);
                        //PrintBlock(&blk[2]);
                        writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
                        out_block++;
                    }
                    if(k == blk[1].nop - 1 && blk[1].next_block == 0){freeBlockInBuffer(blk[1].ptr,buf);break;}
                    else if(k == blk[1].nop - 1){
                        k = -1;
                        freeBlockInBuffer(blk[1].ptr,buf);
                        ReadBlock(&blk[1],blk[1].next_block,buf);
                    }
                }
                if(j == blk[0].nop - 1 && blk[1].next_block == 0){freeBlockInBuffer(blk[0].ptr,buf);break;}
                else if(j == blk[0].nop - 1){
                    j = -1;
                    freeBlockInBuffer(blk[0].ptr,buf);
                    ReadBlock(&blk[0],blk[0].next_block,buf);
                }
            }
        } else {
            if(blk[0].ptr != NULL)freeBlockInBuffer(blk[0].ptr,buf);
            if(blk[1].ptr != NULL)freeBlockInBuffer(blk[1].ptr,buf);
        }
    }
    if(out_count != 0){
        blk[2].nop = out_count / 2;
        blk[2].addr = out_addr + out_block;
        blk[2].next_block = 0;
        RefreshBlock(&blk[2]);
        //PrintBlock(&blk[2]);
        writeBlockToDisk(blk[2].ptr,out_addr + out_block,buf);
    } else if(out_block != 0) {
        blk[2].next_block = 0;
        writeBlockToDisk(blk[2].ptr, out_addr + out_block - 1, buf);
    }
    freeBlockInBuffer(blk[2].ptr,buf);
}

int main(int argc, char **argv) {
    AllBlockSort();
    AllSortR(1);
    AllSortS(1);
    MakeHashBlock(1);
    Buffer buf;
    if (!initBuffer(520, 64, &buf))
    {
        perror("Buffer Initialization Failed!\n");
        return -1;
    }
    //LinearSelect('R',1,40,&buf);
    //LinearSelect('S',1,60,&buf);
    //BinarySelect('R',1,40,&buf);
    //BinarySelect('S',1,60,&buf);
    //HashSearch('R',40,&buf);
    //HashSearch('S',60,&buf);
    //Projection('R',1,&buf);
    //Projection('S',1,&buf);
    //NestLoopJoin(&buf);
    //SortMergeJoin(&buf);
    HashJoin(&buf);
    return 0;
}
