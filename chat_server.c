#include <stdio.h>    // 표준 입출력
#include <stdlib.h>    // 표준 라이브러리
#include <unistd.h>    // 유닉스 표준
#include <string.h>    // 문자열 처리
#include <arpa/inet.h>    // 인터넷 프로토콜
#include <sys/socket.h>    // 소켓함수
#include <netinet/in.h>    // 인터넷 주소 체계 (ex. in_port_t)
#include <pthread.h>

#define BUF_SIZE 256    // 채팅할 때 메시지 최대 길이
#define MAX_CLNT 256    // 최대 동시 접속자 수

void *handle_clnt(void *arg);    // 클라이언트 쓰레드용 함수(함수 포인터)
void *handle_serv(void *arg);    // 클라이언트 쓰레드용 함수(함수 포인터)
void send_msg(char *msg, int len);// 메시지 전달 함수
void send_msg_to_one(char *msg, int len, int clnt_sock);

void error_handling(char *msg);    // 예외 처리 함수

int clnt_cnt = 0;    // 현재 접속중인 사용자 수
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;    // 상호 배제를 위한
char clnt_names[MAX_CLNT][20];
int num_name = 0;

int main(int argc, char *argv[]) {        // 인자로 포트번호 받음
    int serv_sock, clnt_sock;        // 소켓 통신용 서버 소켓과 임시 클라이언트 소켓
    struct sockaddr_in serv_adr, clnt_adr;    // 서버 주소, 클라이언트 주소 구조체
    int clnt_adr_sz;            // 클라이언트 주소 구조체
    pthread_t t_id;            // 클라이언트 쓰레드용 ID
    pthread_t t_id_server;
    // 포트 입력 안했으면
    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);    // 사용법을 알려준다.
        exit(1);                        // 프로그램 비정상 종료
    }

    pthread_mutex_init(&mutx, NULL);    // 커널에서 Mutex 쓰기위해 얻어 온다.
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);    // TCP용 소켓 생성

    memset(&serv_adr, 0, sizeof(serv_adr));    // 서버 주소 구조체 초기화
    serv_adr.sin_family = AF_INET;        // 인터넷 통신
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);    // 현재 IP를 이용하고
    serv_adr.sin_port = htons(atoi(argv[1])); // 포트는 사용자가 지정한 포트 사용

    //서버 소켓에 주소를 할당한다.
    if (bind(serv_sock, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    // 서버 소켓을 서버로 사용
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1) {    // 무한루프 돌면서
        clnt_adr_sz = sizeof(clnt_adr);    // 클라이언트 구조체의 크기를 얻고
        // 클라이언트의 접속을 받아들이기 위해 Block 된다.(멈춘다)
        clnt_sock = accept(serv_sock, (struct sockaddr *) &clnt_adr, &clnt_adr_sz);
        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        pthread_mutex_unlock(&mutx);

        // 클라이언트 구조체의 주소를 쓰레드에 넘긴다.(포트 포함됨)
        pthread_create(&t_id_server, NULL, handle_serv, (void *) &clnt_socks);
        pthread_create(&t_id, NULL, handle_clnt, (void *) &clnt_sock);    // 쓰레드 시작
        pthread_detach(t_id);    // 쓰레드가 종료되면 스스로 소멸되게 함

        // 접속된 클라이언트 IP를 화면에 찍어준다.
        printf("Connected client IP: %s\n", inet_ntoa(clnt_adr.sin_addr));
    }

    close(serv_sock);    // 쓰레드가 끝나면 소켓을 닫아준다.
    return 0;
}

void *handle_serv(void *arg) {
    char msg[BUF_SIZE];    // 메시지 버퍼

    while (1) {
        memset(msg, 0, BUF_SIZE);
        fgets(msg, BUF_SIZE, stdin);

        if (msg[0] == 'a' && msg[1] == 'l' && msg[2] == 'l' && msg[3] == ' ') {
            char *temp = strtok(msg, " ");
            char *new_msg = strtok(NULL, " ");
            if (strcmp(new_msg, "bye\n") == 0) {
                for (int i = 0; i < sizeof(clnt_socks); i++) {
                    close(clnt_socks[i]);
                    clnt_cnt = 0;
                }
                printf("disconnect all clients\n");
            } else {
                char serv_msg[BUFSIZ + 10];
                sprintf(serv_msg, "%s %s", "[server]", new_msg);
                send_msg(serv_msg, sizeof(serv_msg));
            }
        } else {
            char *temp_name = strtok(msg, " ");
            char name[20];
            sprintf(name, "[%s]", temp_name);
            for (int i = 0; i < sizeof(clnt_names); i++) {
                if (strcmp(clnt_names[i], name) == 0) {
                    char *new_msg = strtok(NULL, " ");
                    if (strcmp(new_msg, "bye\n") == 0) {
                        char* bye_msg = "[server] bye";
                        send_msg_to_one(bye_msg, sizeof(bye_msg), clnt_socks[i]);
                        printf("disconnect client %s\n", name);
                        close(clnt_socks[i]);
                        break;
                    } else {
                        char serv_msg[BUFSIZ + 10];
                        sprintf(serv_msg, "%s %s", "[server]", new_msg);
                        send_msg_to_one(serv_msg, sizeof(serv_msg), clnt_socks[i]);
                        break;
                    }
                }
            }
        }

    }

    pthread_mutex_lock(&mutx); // 임계 영역 시작
    pthread_mutex_unlock(&mutx); // 임계 영역 끝
    return NULL;        // 서비스 종료
}

// 쓰레드용 함수
void *handle_clnt(void *arg) {    // 소켓을 들고 클라이언트와 통신하는 함수
    int clnt_sock = *((int *) arg);    // 쓰레드가 통신할 클라이언트 소켓 변수
    int str_len = 0, i;
    char msg[BUF_SIZE];    // 메시지 버퍼
    char clnt_name[20];

    read(clnt_sock, clnt_name, sizeof(clnt_name));
    strcpy(clnt_names[num_name], clnt_name);
    num_name++;

    // 클라이언트가 통신을 끊지 않는 한 계속 서비스를 제공한다.
    int c = 0;
    while ((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        if (strstr(msg, "] bye\n") != NULL) {
            close(clnt_sock);
            if (c == 0) {
                printf("disconnect client %s\n", clnt_name);
                c++;
            }
        } else{
            send_msg(msg, str_len);
        }
    }
    pthread_mutex_lock(&mutx); // 임계 영역 시작

    for (i = 0; i < clnt_cnt; i++) {  // remove disconnected client
        if (clnt_sock == clnt_socks[i]) {
            while (i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }

    clnt_cnt--;
    pthread_mutex_unlock(&mutx); // 임계 영역 끝
    close(clnt_sock);    // 소켓을 닫고
    return NULL;        // 서비스 종료
}

void send_msg(char *msg, int len) {// send to all
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
        write(clnt_socks[i], msg, len);
    pthread_mutex_unlock(&mutx);
}

void send_msg_to_one(char *msg, int len, int clnt_sock) {
    pthread_mutex_lock(&mutx);
    write(clnt_sock, msg, len);
    pthread_mutex_unlock(&mutx);
}


// 예외 처리 함수
void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}