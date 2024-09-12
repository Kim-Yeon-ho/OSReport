#define PAGESIZE (32)
#define PAS_FRAMES (256) //fit for unsigned char Frame in PTE
#define PAS_SIZE (PAGESIZE * PAS_FRAMES) //32*256 = 8192B
#define VAS_PAGES (64)
#define VAS_SIZE (PAGESIZE * VAS_PAGES) // 32*64 = 2048B
#define PTE_SIZE (4) //sizeof(pte)
#define PAGETABLE_FRAMES (VAS_PAGES*PTE_SIZE/PAGESIZE) // 64 * 4/32 = 8 consecutive frames

#define PAGE_INVALID (0)
#define PAGE_VALID (1)

#define MAX_REFERENCES (256)

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "list.c"

typedef struct{
    unsigned char frame; //allocated frame
    unsigned char vflag; // valid-invalid bit
    unsigned char ref; // reference bit
    unsigned char pad; //padding
} pte; //Page Table Entry (total 4Bytes, always)

typedef struct{
    int pid;
    int ref_len; //Less than 255
    unsigned char *reference;
    int pfCnt;  //PageFault가 일어난 횟수
    int rfCnt; //reference가 일어난 횟수
    struct list_head list; // list 구조체를 사용하기 위함
    pte pageTable[VAS_PAGES];
}process_raw;

typedef struct{
    unsigned char b[PAGESIZE];
}frame;

frame PAS[PAS_FRAMES] = {0}; //물리 메모리 생성 및 초기화
process_raw *cur, *next;
LIST_HEAD(job);

int freeFrame;
int proNum = 0;

void loadProcess() {
    // printf("loadProcess() Start\n");
    process_raw data;

     while(fread(&data, sizeof(int)*2, 1, stdin) == 1) {
        cur = malloc(sizeof (process_raw));
        cur->pid = data.pid;
        cur->ref_len = data.ref_len;
        cur->pfCnt = 0;
        cur->rfCnt = 0;
        cur->reference = malloc(sizeof(unsigned char) *cur ->ref_len); //현재 레퍼런스
        // fprintf(stdout, "%d %d\n", cur->pid, cur->ref_len);

        for (int j = 0; j< cur->ref_len; j++){
            fread(&cur->reference[j], sizeof(unsigned char), 1, stdin);
            // fprintf(stdout, "%02d ", cur->reference[j]);
        }

        // puts("");
        INIT_LIST_HEAD(&cur->list);
        list_add_tail(&cur->list, &job); //파일로 부터 process_raw를 입력받아 list에 연결
        proNum++; // while문이 돌때마다 프로세스의 갯수 증가

        for(int i = 0; i<VAS_PAGES; i++) {
            cur->pageTable[i].vflag = PAGE_INVALID;
            cur->pageTable[i].ref = 0;
        }
      
     }
    // printf("loadProcess() end\n");    
    return;
}

void start() {
    // printf("Start() start\n");
    int refCnt = 0;

    while(true) {
        bool isBreak = true;
        list_for_each_entry(cur, &job, list) {
            if(cur->ref_len <= refCnt) continue;
            else isBreak = false;

            // printf("[PID: %02d REF: %03d] Page access %03d: ", cur->pid, refCnt, cur->reference[refCnt]);

            if (cur->pageTable[(cur->reference[refCnt])].vflag == PAGE_INVALID) {
                if(8*proNum + freeFrame >= PAS_FRAMES) {
                    printf("Out of memory!!\n");
                    isBreak = true;
                    break;
                }
                cur->pageTable[(cur->reference[refCnt])].vflag = PAGE_VALID;
                cur->pageTable[(cur->reference[refCnt])].frame = (8*proNum) +freeFrame++;
                cur->pageTable[(cur->reference[refCnt])].ref++;
                cur -> pfCnt++;
                cur -> rfCnt++;
                // printf("PF, Allocated Frame %03d\n", cur->pageTable[(cur->reference[refCnt])].frame);
            }     
            else {
                cur->pageTable[(cur->reference[refCnt])].ref++;
                cur -> rfCnt++;
                // printf("Frame %03d\n", cur->pageTable[(cur->reference[refCnt])].frame);
            }
        }
        if (isBreak) break;
        refCnt++;
    }
    // printf("Start() end\n");
    return;
}

void result() {
    int pageFault = 0;
    int reference = 0;

    list_for_each_entry(cur,&job,list) {
        pageFault += cur->pfCnt;
        reference += cur->rfCnt;

        printf( "** Process %03d: Allocated Frames=%03d PageFaults/References=%03d/%03d\n", cur->pid, cur->pfCnt + 8,cur->pfCnt, cur->rfCnt);

        
        for(int i = 0; i<VAS_PAGES;i++){
            if(cur->pageTable[i].vflag == PAGE_VALID) {
                printf("%03d -> %03d REF=%03d\n",i,cur->pageTable[i].frame,cur->pageTable[i].ref);
            }
        }
    }
     printf("Total: Allocated Frames=%03d Page Faults/References=%03d/%03d\n", (pageFault + (8*proNum)),pageFault,reference);
}

void freeMem() {
    list_for_each_entry_safe(cur,next,&job,list) {
        free(cur->reference);
        list_del(&cur->list);
        free(cur);
    }
}


int main(int argc, char* argv[]) {
    loadProcess();
    start();
    result();
    freeMem();
    return 0;
}