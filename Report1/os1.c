#include <stdio.h>
#include <stdlib.h>
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
} process;

process *cur, *next; // 현재 프로세스와 다음을 가리키는 포인터

LIST_HEAD(job_q);//job_q 초기화 

int main (int argc, char* argv[]) {
    process data;
    code tuple; //1byte씩 데이터를 읽기 위해 char형 선언, 양수만 표현하기 위하여 unsigned char 사용
    while(fread(&data, 12, 1, stdin) == 1) { //프로세스 정보가 12개의 튜플로 되어있음
        cur = calloc(1,sizeof(*cur)); //현재 프로세스를 큐에 저장 해야하니 동적할당을 함
        cur->pid = data.pid;
        cur->arrival_time = data.arrival_time;
        cur->code_bytes = data.code_bytes;// 현재 프로세스의 pid,arrivalTime,codeByte를 동적할당한 객체에 삽입함.
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
    //역순 출력
     list_for_each_entry_safe_reverse(cur, next, &job_q, job) {
        printf("PID: %03d\tARRIVAL: %03d\tCODESIZE: %03d\n", cur->pid, cur->arrival_time,cur->code_bytes);
        for(int i=0; i<cur->code_bytes/2; i++) {
            printf("%d %d\n", cur->operations[i].op,cur->operations[i].len);
        }
        //동적할당후 해제
        list_del(&cur->job);
        list_del(&cur->ready);
        list_del(&cur->wait);
        //현재 객체의 각각 list를 삭제
        free(cur->operations);
        free(cur);
     }
     return 0;
}