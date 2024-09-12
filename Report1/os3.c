#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "list.c"

typedef struct {
    unsigned char op;
    unsigned char len;
} code;

typedef struct {
    int pid; //ID
    int arrival_time; //도착시간
    int code_bytes; //코드길이(바이트)
    code *operations; //코드튜플의 위치
    struct list_head job, ready, wait;
    int pc;// 코드의 실행 위치를 알기 위한 program counter
    bool loaded;
} process;

process *cur, *next; // 현재 프로세스와 다음을 가리키는 포인터

LIST_HEAD(job_q);//job_q 초기화
LIST_HEAD(ready_q);// ready_q 초기화
LIST_HEAD(wait_q);//wait_q 초기화

int clocks = 0 ; //total clock의 수
int endClocks = 0;
int idleTime = 0; //idle모드의 총 시간
int prevPID = 0;
bool isSwitching = false;

//job_q에 데이터를 읽고 미리 로드하는 함수
void roadProcess(process *cur) {
    process data;
    code tuple; //1byte씩 데이터를 읽기 위해 char형 선언, 양수만 표현하기 위하여 unsigned char 사용
    while(fread(&data, 12, 1, stdin) == 1) { //프로세스 정보가 12개의 튜플로 되어있음
        cur = calloc(1,sizeof(*cur)); //현재 프로세스를 큐에 저장 해야하니 동적할당을 함
        cur->pid = data.pid;
        cur->arrival_time = data.arrival_time;
        cur->code_bytes = data.code_bytes;// 현재 프로세스의 pid,arrivalTime,codeByte를 동적할당한 객체에 삽입함.
        cur->pc = 0; //초기 시작위치는 0
        cur->loaded = false;
        INIT_LIST_HEAD(&cur->job);
        INIT_LIST_HEAD(&cur->ready);
        INIT_LIST_HEAD(&cur->wait);
        list_add_tail(&cur->job, &job_q); //cur객체의 job 큐를 job_q에 연결함

        cur->operations = calloc(data.code_bytes/2, sizeof(cur->operations)); //코드튜플을 저장하기 위해 cur의 oper 동적할당함
    
        for(int i = 0; i <(data.code_bytes/2); i++) { //튜플의 크기는 2byte이고, data의 code_byte는 4byte이기에 절반만큼 크기 지정
            fread(&tuple, sizeof(code), 1, stdin); //1byte만큼 파일 데이터를 읽음, 처음 1byte의 동작정보를 저장
            cur->operations[i].op = tuple.op;
            cur->operations[i].len = tuple.len;
        }
    }
    return;
}

//idle 프로세스 생성해야함 -> job_q에 로드
void roadingIdle() {
    process *idle = calloc(1,sizeof(*cur));
    idle->pid = 100;
    idle->arrival_time = 0;
    idle->code_bytes = 2;
    idle->pc = 0;
    INIT_LIST_HEAD(&idle->job);
    INIT_LIST_HEAD(&idle->ready);
    INIT_LIST_HEAD(&idle->wait);
    list_add_tail(&idle->job, &job_q);
    idle->operations = calloc(1,sizeof(idle->operations));
    idle->operations->op = 255; //0xff
    idle->operations->len = 0;
    return;
}
//idle 프로세스는 다른 프로세스가 모두 wait일때 -> ready 큐에 idle만 있을 때 돌아감, 다른 프로세스가 있으면 idle 프로세스에서 나와야 함

//할당한 프로세스 해제해주는 함수 필요
void deleteProcess() {
    list_for_each_entry_safe(cur, next, &job_q, job) {
        list_del(&cur->job); //job_q연결 해제
        // free(cur->operations);// 할당한 operation 해제
        // free(cur);// 프로세스 해제
    }
    return;
}
//Operation
// arrivalTime이 되면 job큐에서 ready큐로
//operation함수 필요

void runOperation(process *cur) {
    code tuple = cur->operations[cur->pc];
    
    if(tuple.op == 0) {
        printf("%04d CPU: OP_CPU START len: %03d ends at: %04d\n", clocks,tuple.len, clocks+tuple.len);
        endClocks = clocks+tuple.len;
    }
    else if (tuple.op == 1){
        //io작업인데 wait큐로 들어가면 프로세스가 idle 모드가 되니까 그 시간만큼 idleTime 증가
        printf("%04d CPU: OP_IO START len: %03d ends at: %04d\n", clocks,tuple.len, clocks+tuple.len);
        clocks++;
        cur->arrival_time = clocks+tuple.len;
        endClocks = clocks+10;
        isSwitching = true;
        prevPID = cur->pid;
        //이때 idle모드로 들어가야 함
        list_add_tail(&cur->wait, &wait_q);
        list_del(&cur->ready);
    }
}
  
void insertReady(int clocks) {
//arrivalTime == clock job ->cur -> ready
    list_for_each_entry(cur, &job_q, job) {
        if( cur->arrival_time <= clocks && !(cur->loaded)  ) {
            list_add_tail(&cur->ready, &ready_q);
            printf("%04d CPU: Loaded PID: %03d\tArrival: %03d\tCodesize: %03d\tPC: %03d\n", clocks, cur->pid, cur->arrival_time, cur->code_bytes, cur->pc);
            cur->loaded = true;
        }
      }
    list_for_each_entry(cur, &wait_q, wait) {
        if( cur->arrival_time <= clocks) {
            list_add_tail(&cur->ready, &ready_q);
            printf("%04d IO : COMPLETED! PID: %03d\tIOTIME: %03d\tPC: %03d\n", clocks, cur->pid, clocks, cur->pc);
            
            list_del(&cur->wait);

        }
    }
}

void simulator() {
    while(true) {
        if (clocks == endClocks && isSwitching) {
            printf("%04d CPU: Switched\tfrom: %03d\tto: %03d\n",clocks, prevPID, cur->pid);
                    isSwitching = false;
        }
      //clock이 0 일때  맨처음 0번 프로세스, idle 프로세스, 1번 3초일때, 2번 16초일때 3초일때는 idle이 아니고 16초일때는 idle상태임
        
        insertReady(clocks); //readyq에 도착한 프로세스를 저장함
        //0번의 4초짜리 cpu작업을 시행해야함
        
        cur = list_entry((&ready_q)->next, typeof(*cur), ready); // cur에 ready의 첫번째 프로세스 저장

        //readyq에 작업 할 프로세스가 있으면 op와 Len을 확인하여 작업해야함 -> runOperation()
        if(cur->pid==100) {
            int count = 0;
            list_for_each_entry(cur,&ready_q,ready) {
                //idle상태인데 종료조건 추가해야함 readyQ에 idle만 있을 때. 리스트를 순회한다.
                //1개남았을 때가 종료임
                count++;
            }
            list_for_each_entry(cur,&wait_q,wait) {
                count++;
            }
            if(count == 1) {
                endClocks-=10;
                idleTime-=10;
                break;
            }
            else  {
                cur = list_entry(((&ready_q)->next)->next, typeof(*cur), ready);
            }
        }
        
        if(!list_empty(&ready_q)) {

            if (clocks == endClocks) {
                runOperation(cur);
                if(!isSwitching) {
                    cur->pc++;
                }
                else {
                    continue;
                }
            }
            //pc의 크기가 코드의 수보다 크면 contextswitching
             if(cur->pc >= (cur->code_bytes)/2) {
                list_del(&cur->ready);
                free(cur->operations);
                free(cur);
                endClocks += 10;
                idleTime += 10;
            }
        }
        clocks++;
        // if(clocks >= 50) break;
    }
    printf("*** TOTAL CLOCKS: %04d IDLE: %04d UTIL: %2.2f%%\n",clocks,idleTime,(double)(clocks-idleTime)/clocks*100);
}

int main (int argc, char* argv[]) {
    roadProcess(cur);
    roadingIdle();
    simulator();
    deleteProcess();
    return 0;
}