#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "pa1.h"
#include "ipc.h"
#include "common.h"


#define PROCESS_MAX 10


int pro_num;
int childpid[PROCESS_MAX+1];
int fd[PROCESS_MAX+1][PROCESS_MAX+1][2];

int init_pipes(int fd[PROCESS_MAX+1][PROCESS_MAX+1][2], FILE *pipes_log){
	
	for(int i = 0; i<=pro_num;i++){
		for(int j =0;j<=pro_num;j++){
			if(i==j){
				fd[i][j][0]=-1;
				
				fd[i][j][1]=-1;
			}else{
				if (pipe(fd[i][j]) < 0) {
				printf("error: make ipc channel fail!\n");
				return -1;
				}else{
					fprintf(pipes_log, "Pipe number%d and %d was created.\n",i,j);
					
				}
			
				
			}
			
			
		}
		
		
	}
	return 0;
}
void close_pipe(local_id id, FILE *pipes_log, int fd[PROCESS_MAX+1][PROCESS_MAX+1][2]){
	for (local_id m = 0; m <= pro_num; m++){
		for(local_id n = 0; n <= pro_num; n++){
			
			if (m != n) {
				if (m == id) {
					close(fd[m][n][0]);
					fprintf(pipes_log, "PID:%d closed read: %hhd -- %hhd\n", id , m , n);
				}

				if (n == id) {
					close(fd[m][n][1]);
					fprintf(pipes_log, "PID:%d closed write: %hhd -- %hhd\n", id , m , n);
				}

				if (m != id && n != id) {
					close(fd[m][n][0]);
					close(fd[m][n][1]);
					fprintf(pipes_log, "PID:%d closed pipe: %hhd -- %hhd\n", id , m , n);
				}
			}
		}
	}
	
	fprintf(pipes_log, "PID:%d closed all file descriptors.\n", id);
	
}
int send(void * self, local_id dst, const Message * msg){
	
    local_id src  = *((local_id*)self);
    if (src == dst) return -1;
    int fd1   = fd[src][dst][1];
    return write(fd1, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
	
}
int receive(void *self, local_id from, Message *msg) {
    
    local_id dst  = *((local_id*)self);
    if (dst == from) return -1;
    int fd1   = fd[from][dst][0];
	
    int read_result = read(fd1, msg, sizeof(Message));
    if (read_result < 0) {
        perror("read");
        return -1;
    }
    return read_result > 0 ? 0 : -1;
}
int send_multicast(void * self, const Message * msg) {
	for (local_id i = 0; i <= pro_num; i++) {
        send(self, i, msg);
		
			
		//printf("===%s",msg->s_payload);
    }
    return 0;
}
/**
int receive_any(void *self , Message *msg){
    for(int i = 0 ; i < pro_num ; i++){
        if(receive(self, i, msg) == 0)
            return 0;
    }
    return -1;
}
**/

int done_work(local_id pid, FILE *pipes_log, FILE *events_log){
	size_t len;
	
	len = fprintf(events_log,log_done_fmt,pid);
	if(len<=0){
		printf("error");
		return -1;
	}
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = len;
    msg.s_header.s_type = DONE;
	if(pid!=0)
		send_multicast(&pid, &msg);
	
	for (local_id i = 1; i <= pro_num; i++) {
		
       if (i != pid)
           if(receive(&pid, i, &msg) != 0){
			   fprintf(events_log,"Process %d can't received.\n", pid);
			   return -1;
		   }
			   
    }
	
	fprintf(events_log, log_received_all_done_fmt, pid);
	
	return 0;
	
}
int start_init(local_id pid, FILE *pipes_log, FILE *events_log){
	
	size_t len;
	close_pipe(pid, pipes_log, fd);
	len = fprintf(events_log,log_started_fmt, pid, getpid(), getppid());
	
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = len;
    msg.s_header.s_type = STARTED;
	
	send_multicast(&pid, &msg);
		
	
		
	
	for (local_id i = 1; i <= pro_num; i++) {
       if (i != pid)
           while(receive(&pid, i, &msg) != 0);
    }
	
	
	fprintf(events_log, log_received_all_started_fmt, pid);
	
	
	return 0;
}


int main(int argc, char **argv){
	FILE *pipes_log = NULL;
	FILE *events_log = NULL;
	if(argc<2){
		printf("param is not correct!");
		return -1;
	}
	
	pro_num = atoi(argv[2]); 
	if(pro_num < 0) return -1;
	
	
	pipes_log = fopen("pipes.log","w");
	if(pipes_log == NULL){
		 //perror("can't open/create pipes.log file (fopen)");
		return -1;
	}
	
	events_log = fopen("events.log","w+");
	if(events_log == NULL){
		 //perror("failed open events.log file");
		return -1;
	}
	
	
	
	if(init_pipes(fd, pipes_log)!=0)
		exit(1);

	fflush(pipes_log);
	childpid[0]=getpid();
	
	local_id pid;
	for(int i=1;i <= pro_num;i++){
	 pid = fork();
	if(pid==0){
			childpid[i] =getpid();
			break;
		}
		
	}
	//printf("creat 11111 %d %d",childpid[0],getpid());
	if(getpid()==childpid[0]){
		
		int r1 = start_init(0,pipes_log,events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", 0);
			}
			
			int r2 = done_work(0,pipes_log,events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", 0);
			}
			
		
			
		int child_num = pro_num;//子进程个数
		while(child_num --){
			int pr = wait(NULL);
			fprintf(events_log,"Process %d exit.\n", pr);
			
			
		}
			
		close_pipe(0, pipes_log, fd);
	
		fclose(pipes_log);
		fclose(events_log);
		return 0;
	}else{
		for(int i =1;i<=pro_num;i++){
		if(getpid()==childpid[i]){
			
			int r1 = start_init(i,pipes_log,events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", i);
			}
			/**
			
			**/
			sleep(3);
			int r2 = done_work(i,pipes_log,events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", i);
			}
			
			
			
			
			
			
			
			
			
			exit(0);
			
		}
		
		}
		
		
	}
	
	
}

