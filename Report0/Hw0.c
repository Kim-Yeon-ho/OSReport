#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int pid; //ID
    int arrival_time; //도착시간
    int code_bytes; //코드길이(바이트)
} process;

int main (int argc, char* argv[]) {
    process data;
    unsigned char tupleLen, tupleOp; //1byte씩 데이터를 읽기 위해 char형 선언, 양수만 표현하기 위하여 unsigned char 사용

    while(fread(&data, sizeof(process), 1, stdin) == 1) {
        fprintf(stdout,"%d %d %d\n", data.pid, data.arrival_time, data.code_bytes); //프로세스의 정보 출력
        for(int i = 0; i <(data.code_bytes/2); i++) { //튜플의 크기는 2byte이고, data의 code_byte는 4byte이기에 절반만큼 크기 지정
            fread(&tupleOp, sizeof(unsigned char), 1, stdin); //1byte만큼 파일 데이터를 읽는다.
            fread(&tupleLen, sizeof(unsigned char), 1, stdin);
            fprintf(stdout,"%d %d\n",tupleOp,tupleLen);
        }
    }
}
return 0;