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
}process_raw;

typedef struct{
    unsigned char b[PAGESIZE];
}frame;

frame *pas;
process_raw *cur, *next;
LIST_HEAD(job);

int freeFrame; //free상태의 index시작위치
int proNum = 0; //프로세스 수

//이진 파일로부터 process_raw를 입력받아 list에 연결해주는 함수
//while문이 한번씩 돌때마다 프로세스의 수 1씩 증가
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
    
        //pas 동적할당 초기화
        pas = (frame*)malloc(PAS_SIZE); //8196b, 256크기 배열
        freeFrame = proNum ;//1frame으로 할당
        for(int i = 0; i<(freeFrame) ; i++) {
            pte *curPte  = (pte*)&pas[i]; //페이지 테이블은 PAS에서
            for (int j = 0; j<8; j++) { // 8개의 페이지엔트리 초기화
                curPte[j].vflag = PAGE_INVALID;
                curPte[j].ref = 0;
            }
        }
        
     }
    // printf("loadProcess() end\n");    
    return;
}

void start() {
    // printf("Start() start\n");
    int refCnt = 0; //process_raw 배열에 접근할 인덱스 번호
    pte *curPteLv1; 
    pte *curPteLv2; //pte*형 변수

    while(true) {
        bool isBreak = true;
        list_for_each_entry(cur, &job, list) {
            if(cur->ref_len <= refCnt) continue;
            else isBreak = false;
            curPteLv1  = (pte*)&pas[cur->pid]; //pas 물리 메모리 배열에서 현재 프로세스에 해당하는 frame 의 주소를 pte* 으로 캐스팅 -> 현재 프로세스의 1단계 PTE를 가리키게 됨
            // printf("[PID: %02d REF: %03d] Page access %03d: ", cur->pid, refCnt, cur->reference[refCnt]);

            if (curPteLv1[(cur->reference[refCnt])/8].vflag == PAGE_INVALID) { // curPteLv1[(cur->reference[refCnt])/8]의 값 접근으로 lv1 계층
                if(freeFrame >= PAS_FRAMES) {
                    printf("Out of memory!!\n"); //pagefault접근 후 OutOfMemory인지 확인
                    isBreak = true;
                    break;
                }
                //lv1 pte의 vflag,frame값 변경, 현재 접근한 process_rawd의 pageFaultCount 증가
                curPteLv1[(cur->reference[refCnt])/ 8].vflag = PAGE_VALID;
                curPteLv1[(cur->reference[refCnt])/ 8].frame = freeFrame++;
                cur -> pfCnt++;

                // printf("PF, Allocated Frame %03d\n", cur->pageTable[(cur->reference[refCnt])].frame);
            }     
            else {
                // cur->pageTable[(cur->reference[refCnt])].ref++;
                // cur -> rfCnt++;
                // printf("Frame %03d\n", cur->pageTable[(cur->reference[refCnt])].frame);
                //lv1 에서 pagefault가 안 일어날 시아무 동작 안함
            }

            curPteLv2 = (pte*)&pas[curPteLv1[(cur->reference[refCnt])/ 8].frame]; //curPteLv1[(cur->reference[refCnt])/8]의 frame정보를 pas의 시작지점으로 가지는 lv2

            if (curPteLv2[(cur->reference[refCnt]) % 8].vflag == PAGE_INVALID) { //(curPteLv2[(cur->reference[refCnt]) % 8]의 값 접근으로 lv2 계층
                if(freeFrame >= PAS_FRAMES) {
                    printf("Out of memory!!\n");//pagefault접근 후 OutOfMemory인지 확인
                    isBreak = true;
                    break;
                }
                //lv2 pte의 vflag,frame 값 변경 및 reference값 1로 초기화
                //현재 접근한 process_raw의 pageCount,referenceCount 증가
                curPteLv2[(cur->reference[refCnt]) % 8].vflag = PAGE_VALID;
                curPteLv2[(cur->reference[refCnt]) % 8].frame = freeFrame++;
                curPteLv2[(cur->reference[refCnt]) % 8].ref = 1;
                cur -> pfCnt++;
                cur -> rfCnt++;
            }
            else {
                //lv2 pte의 reference 값 증가 및 현재 접근한 process_raw의 reference값 증가
                curPteLv2[(cur->reference[refCnt]) % 8].ref++;
                cur -> rfCnt++;
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
        reference += cur->rfCnt;//joblist 순회하면서 현재 접근한 process_raw의 pagefault,reference Count를 더해줌

        //allocatedFrame은 lv1 pagetable의 크기로 1증가
        printf( "** Process %03d: Allocated Frames=%03d PageFaults/References=%03d/%03d\n", cur->pid, cur->pfCnt+1 ,cur->pfCnt, cur->rfCnt);

        //현재 프로세스의 lv1 위치에서 pte크기 만큼 for문
        pte *curPteLv1 = (pte*)&pas[cur->pid];
        for(int i = 0; i<8;i++){
            if(curPteLv1[i].vflag == PAGE_VALID) {
                //lv1의 현재 엔트리가 Valid하다면 출력
                printf("(L1PT) %03d -> %03d\n",i,curPteLv1[i].frame);
                //lv2의 초기위치를 valid한 값의 frame에서 시작
                pte *curPteLv2 = (pte*)&pas[curPteLv1[i].frame];
                //frame의 크기만큼 반복하며 lv2의 엔트리가 valid하다면 출력
                for(int j=0; j<8; j++) {
                    if(curPteLv2[j].vflag == PAGE_VALID) {
                        printf("(L2PT) %03d -> %03d REF=%03d\n", i*8 + j, curPteLv2[j].frame, curPteLv2[j].ref);
                    }
                }
            }
        }
    }//프로세스마다 lv1 pagetable의 크기로 1증가
     printf("Total: Allocated Frames=%03d Page Faults/References=%03d/%03d\n", pageFault+ (1*proNum) ,pageFault,reference);
}

void freeMem() {
    list_for_each_entry_safe(cur,next,&job,list) {
        free(cur->reference); //ref할당해제
        list_del(&cur->list); //job_q연결 해제
        free(cur); //process 할당해제
    }
    free(pas); //물리메모리 할당해제
}


int main(int argc, char* argv[]) {
    loadProcess();
    start();
    result();
    freeMem();
    return 0;
}