#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include<errno.h>
#include "ipc.h"
#include "common.h"
#include "pa2345.h"
#include "banking.h"
#define PROCESS_MAX 10


int pro_num;
int childpid[PROCESS_MAX+1];
int fd[PROCESS_MAX+1][PROCESS_MAX+1][2];
int init_balance[PROCESS_MAX+1];
//int ack_num =0;
static timestamp_t lamport_time = 0;

timestamp_t get_lamport_time(){
	return lamport_time;
}
timestamp_t add_lamport_time(){
	return ++lamport_time;
}

timestamp_t set_lamport_time_from_msg(Message* msg){
	if (lamport_time < msg->s_header.s_local_time){
		lamport_time = msg->s_header.s_local_time;
	}
	
	return add_lamport_time();
}
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
					int flags0 = fcntl(fd[i][j][0],F_GETFL);  // 鑾峰彇鍘熷厛flag
					flags0 |= O_NONBLOCK;  // 淇敼flag
					fcntl(fd[i][j][0], F_SETFL, flags0);
					int flags1 = fcntl(fd[i][j][1],F_GETFL);  // 鑾峰彇鍘熷厛flag
					flags1 |= O_NONBLOCK;  // 淇敼flag
					fcntl(fd[i][j][1], F_SETFL, flags1);
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
					//fprintf(stdout, "PID:%d closed read: %hhd -- %hhd\n", id , m , n);
				}

				if (n == id) {
					close(fd[m][n][1]);
					fprintf(pipes_log, "PID:%d closed write: %hhd -- %hhd\n", id , m , n);
					//fprintf(stdout, "PID:%d closed write: %hhd -- %hhd\n", id , m , n);
				}
				
					if (m != id && n != id) {
					close(fd[m][n][0]);
					close(fd[m][n][1]);
					fprintf(pipes_log, "PID:%d closed pipe: %hhd -- %hhd\n", id , m , n);
					//fprintf(stdout, "PID:%d closed read: %hhd -- %hhd\n", id , m , n);
					}	
					
					
								
			}
		}
	}
	
	//fprintf(pipes_log, "PID:%d closed all file descriptors.\n", id);
	
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
	
    int status = read(fd1, msg, sizeof(MessageHeader));
	//printf("%d from %d, read type=%d s_payload_len=%d  result %d\n",dst,from ,msg->s_header.s_type, msg->s_header.s_payload_len, status);
    //sleep(1);
	if (status < 0) {
		if(status == -1 && errno == EAGAIN){
			
			return -1;
		}
	}
	if(status >0){
		
		read(fd1, msg->s_payload, msg->s_header.s_payload_len);
		
	}
		//perror("read");
    
	printf("%d read type=%d len=%d from %d result %d\n",dst, msg->s_header.s_type, msg->s_header.s_payload_len,from, status);
    
    return (status == -1 || status == 0) ? -1 : 0;
}
int send_multicast(void * self, const Message * msg) {
	for (local_id i = 0; i <= pro_num; i++) {
		//fprintf(stdout, "%d send msg to %d\n", *(local_id*)self, i);
		if(i != *(local_id*)self){
			send(self, i, msg);
			fprintf(stdout, "%d send msg len=%d type=%d to %d \n", *(local_id*)self, msg->s_header.s_payload_len, msg->s_header.s_type, i);
			
			
		}
        
		//printf("===%s",msg->s_payload);
    }
    return 0;
}

int receive_any(void *self , Message *msg){
	
	int i;
    for(i = 0 ; i <= pro_num ; i++){
		
		if(i==*(local_id*)self)
			continue;
        if(receive(self, i, msg) == 0){
			return i;
		}
            
    }
    return -1;
}


int done_work(BalanceHistory* history, local_id pid, FILE *pipes_log, FILE *events_log){
	
	size_t len;
	
	len = fprintf(events_log,log_done_fmt,get_lamport_time(), pid, history->s_history[history->s_history_len-1].s_balance);
	if(len<=0){
		printf("error");
		return -1;
	}
	
	
	if(pid!=0){
		add_lamport_time();
		Message msg;
		msg.s_header.s_magic = MESSAGE_MAGIC;
		msg.s_header.s_payload_len = 0;
		msg.s_header.s_type = DONE;
		msg.s_header.s_local_time = get_lamport_time();
		send_multicast(&pid, &msg);
		add_lamport_time();
		
	}
	Message msg1;
	msg1.s_header.s_magic = MESSAGE_MAGIC;
    msg1.s_header.s_type = DONE;
	
	for (local_id i = 1; i <= pro_num; i++) {
		
       if (i != pid&&pid!=0){
		   fprintf(stdout,"%d receive %d from:%d\n",pid, msg1.s_header.s_type, i);
		   while(receive(&pid, i, &msg1) < 0);
		   
	   }
	   else if(pid==0){
		   //sleep(4);
		   fprintf(stdout,"%d receive %d from:%d\n",pid, msg1.s_header.s_type, i);
		   while(receive(&pid, i, &msg1) < 0);
	   }
           
	
    }
	set_lamport_time_from_msg(&msg1);
	fprintf(events_log, log_received_all_done_fmt,get_lamport_time(), pid);
	
	return 0;
	
}
int start_init(BalanceHistory* history, local_id pid, FILE *pipes_log, FILE *events_log){
	
	size_t len;
	
	close_pipe(pid, pipes_log, fd);
	
	
	
	//history.s_history[0].s_balance = (*all_history).s_history[pid].s_history[0].s_balance;
	history->s_history_len=1;
	history->s_history[0].s_time = get_lamport_time(); 
	history->s_history[0].s_balance_pending_in = history->s_history[0].s_balance;
	len = fprintf(events_log,log_started_fmt,get_lamport_time(),pid,getpid(),getppid(),history->s_history[0].s_balance);
	fprintf(stdout,log_started_fmt,get_lamport_time(),pid,getpid(),getppid(),history->s_history[0].s_balance);
	
	if(pid!=0){
		add_lamport_time();
		Message msg;
		msg.s_header.s_magic = MESSAGE_MAGIC;
		msg.s_header.s_payload_len = 0;
		msg.s_header.s_type = STARTED;
		msg.s_header.s_local_time = get_lamport_time();
		send_multicast(&pid, &msg);
		add_lamport_time();
		
	}
		
	
	Message msg1;
    msg1.s_header.s_type = STARTED;
	msg1.s_header.s_magic = MESSAGE_MAGIC;

	//msg1.s_header.s_local_time = get_lamport_time();
	
	for (local_id i = 1; i <= pro_num; i++) {
       if (i != pid){
		   while(receive(&pid, i, &msg1) < 0);
		   
		   
	   }
           
    }
	
	set_lamport_time_from_msg(&msg1);
	fprintf(events_log, log_received_all_started_fmt,get_lamport_time(), pid);
	fprintf(stdout, log_received_all_started_fmt,get_lamport_time(), pid);
	
	
	return 0;
}
void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    // student, please implement me
	//sleep(1);
	//printf("tttttttttttttttttttttttttttttttt    is  %d\n",time);
	add_lamport_time();
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_payload_len = sizeof(TransferOrder);
	msg.s_header.s_type = TRANSFER;
	msg.s_header.s_local_time = get_lamport_time();
	TransferOrder order = (TransferOrder){
		.s_src = src,
		.s_dst = dst,
		.s_amount = amount
	};
	memcpy(msg.s_payload, &order, sizeof(TransferOrder));
	send((local_id*)parent_data, src , &msg);
	
	printf("parent send=============%d\n",src);
	
	
	Message msg1;
	
	while(receive((local_id*)parent_data, dst, &msg1)<0||msg1.s_header.s_type != ACK);
	set_lamport_time_from_msg(&msg1);
	
}
void update_history(BalanceHistory* history, balance_t amount){
	
	int t = get_lamport_time();
    
	
	
	for(int i = history->s_history_len; i < t; i++){
		history->s_history[i].s_time = history->s_history[i-1].s_time + 1;
		history->s_history[i].s_balance = history->s_history[i-1].s_balance;
		history->s_history[history->s_history_len].s_balance_pending_in = history->s_history[history->s_history_len-1].s_balance;
		printf("%d:  curr :%d   and  prev balance: %d\n",t,history->s_history[history->s_history_len].s_balance_pending_in,history->s_history[history->s_history_len-1].s_balance_pending_in);
	
		history->s_history_len++;
	}
	
	history->s_history[history->s_history_len].s_time = t;
	
	history->s_history[history->s_history_len].s_balance = history->s_history[history->s_history_len-1].s_balance;
	history->s_history[history->s_history_len].s_balance_pending_in = history->s_history[history->s_history_len].s_balance;
	//printf("ttttttttt====%d===\n",t);
		
	printf("prev====%d===\n",history->s_history[history->s_history_len-1].s_balance_pending_in);
	printf("prev==stime==%d===\n",history->s_history[history->s_history_len].s_time);
	
	history->s_history[history->s_history_len].s_balance += amount;
	printf("behind====%d===\n",history->s_history[history->s_history_len].s_balance_pending_in);
	printf("behind==stime==%d===\n",history->s_history[history->s_history_len].s_time);
	history->s_history_len++;
	
}



int child_do_work(BalanceHistory* history, local_id pid, FILE *pipes_log, FILE *events_log){
		
		Message msg;
		
		while(1){
			
			int i = -1;
			if((i = receive_any(&pid, &msg))<0){
				continue;
			}
			
			
			if(msg.s_header.s_type==TRANSFER){
				TransferOrder order = *((TransferOrder*)msg.s_payload);
				printf("%d:%d tranfer money %d\n",get_lamport_time(),pid, order.s_amount);
				if(order.s_src==pid){
					set_lamport_time_from_msg(&msg);
					add_lamport_time();
					msg.s_header.s_local_time = get_lamport_time();
					//printf("%d:pid %dlength is %d\n",get_lamport_time(), pid, history->s_history_len);
					update_history(history, -order.s_amount);
					fprintf(events_log,log_transfer_out_fmt, get_lamport_time(), pid, order.s_amount, order.s_dst);
					
					send((local_id*)&order.s_src, order.s_dst, &msg);
					//printf("%d:>>>>>>>>>>>>>>>>>%d child decrease to %d\n",get_lamport_time(),pid ,history->s_history[history->s_history_len-1].s_balance);
					//printf("%d:pid %dlength is %d\n",get_lamport_time(), pid, history->s_history_len);
				}
				else if(order.s_dst==pid){
					//printf("%d:pid %dlength is %d\n",get_lamport_time(), pid, history->s_history_len);
					lamport_time = msg.s_header.s_local_time;
					update_history(history, order.s_amount);
					set_lamport_time_from_msg(&msg);
					fprintf(events_log,log_transfer_in_fmt, get_lamport_time(), pid, order.s_amount, order.s_src);
					//printf("%d:<<<<<<<<<<<<<<<<<<%d child add update %d\n",get_lamport_time(),pid ,history->s_history[history->s_history_len-1].s_balance);
					//printf("%d:pid %dlength is %d\n",get_lamport_time(), pid, history->s_history_len);
					add_lamport_time();
					Message msg1;
					msg1.s_header.s_magic = MESSAGE_MAGIC;
					msg1.s_header.s_type = ACK;
					msg1.s_header.s_local_time = get_lamport_time();
					send((local_id*)&order.s_dst, 0, &msg1);
					//printf("%d:child send ack %d \n",get_lamport_time(), order.s_dst);
					
				}
				
			}
			else if(msg.s_header.s_type==STOP){
				//printf("child STOP %d\n",pid);
				set_lamport_time_from_msg(&msg);
				for(int k = history->s_history_len; k <= get_lamport_time(); k++){
					history->s_history[k].s_time = history->s_history[k-1].s_time + 1;
					history->s_history[k].s_balance = history->s_history[k-1].s_balance;
					history->s_history[k].s_balance_pending_in = history->s_history[k-1].s_balance;
					history->s_history_len++;
				}
				printf("%d:  pid=%d  STOP time is %d \n",get_lamport_time(),pid,history->s_history[history->s_history_len-1].s_time);
				printf("%d: pid=%d final len = %d\n",get_lamport_time(),pid,history->s_history_len);
				add_lamport_time();
				Message msg2;
				msg2.s_header.s_magic = MESSAGE_MAGIC;
				msg2.s_header.s_type = BALANCE_HISTORY;
				msg2.s_header.s_payload_len = sizeof *history - (MAX_T + 1 - history->s_history_len) * sizeof *history->s_history;
				msg2.s_header.s_local_time = get_lamport_time();
				//msg2.s_payload = &history;
				memcpy(msg2.s_payload, history, sizeof(BalanceHistory));
				
				//printf("%d\n",*(BalanceHistory*)(msg2.s_payload).s_history[0].s_balance);
				send(&pid, 0, &msg2);
				return 0;
			}
			
		}
		
		
		
		return -1;
		
		
	
	
	
	
		
		
		
		
		
	
	return -1;
}
int main(int argc, char **argv){
	FILE *pipes_log = NULL;
	FILE *events_log = NULL;
	AllHistory all_history;
	
	pro_num = atoi(argv[2]);
	
	if(pro_num < 0) return -1;
	if(argc<(2+pro_num)&& (strcmp(argv[1], "-p") != 0)){
		printf("param num is not correct!\n");
		return -1;
	}
	
	all_history.s_history_len = pro_num+1;
	
	for(int i=0;i<pro_num;i++){
		init_balance[i+1] = (balance_t)atoi(argv[i+3]);
		//printf("%d\n", (balance_t)atoi(argv[i+3]));
		if(init_balance[i+1]<0){
			printf("init balance is negative!\n");
			return -1;
		}
		all_history.s_history[i+1].s_id = i+1;
		all_history.s_history[i+1].s_history[0].s_balance = init_balance[i+1];
	}
	//printf("%d\n", (balance_t)atoi(argv[3]));
	
	
	
	
	
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
	
	
	if(getpid()==childpid[0]){
		
		int r1 = start_init(&all_history.s_history[0], 0, pipes_log, events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", 0);
			}
		
		int i = 0;
		
		bank_robbery((local_id*)&i, pro_num);
		//parent_do_work(0,pipes_log,events_log);
		Message msg;
		
		add_lamport_time();
		for(int j = 1;j<=pro_num;j++){
			Message msg3;
			msg3.s_header.s_magic = MESSAGE_MAGIC;
			msg3.s_header.s_payload_len = 0;
			msg3.s_header.s_type = STOP;
			msg3.s_header.s_local_time = get_lamport_time();
			send((local_id*)&i, j, &msg3);
		}
		add_lamport_time();
		for(int j = 1;j<=pro_num;j++){
			
			
			
			while(receive((local_id*)&i, j, &msg)<0||msg.s_header.s_type != BALANCE_HISTORY);
			
			all_history.s_history[j] = *(BalanceHistory*)msg.s_payload;
			
		}
		set_lamport_time_from_msg(&msg);
		
		int r2 = done_work(&all_history.s_history[0], 0, pipes_log, events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", 0);
			}
		/**
			for (i = 1; i <= pro_num; i++){
		BalanceHistory balance_history;
		Message msg;
		
		while(receive(0, i, &msg));
		
		if (msg.s_header.s_type != BALANCE_HISTORY){
			break;
		}
		
		memcpy((void*)&balance_history, msg.s_payload, sizeof(char) * msg.s_header.s_payload_len);
		all_history.s_history[i - 1] = balance_history;
		}
		**/
		
		
		
			
		int child_num = pro_num;//瀛愯繘绋嬩釜鏁?
		while(child_num --){
			int pr = wait(NULL);
			fprintf(events_log,"Process %d exit.\n", pr);
			
			
		}
		print_history(&all_history);
		
		
		close_pipe(0, pipes_log, fd);
		fclose(pipes_log);
		fclose(events_log);
		return 0;
	}else{
		for(int i =1;i<=pro_num;i++){
		if(getpid()==childpid[i]){
			BalanceHistory balance_history = all_history.s_history[i];
			int r1 = start_init(&balance_history, i, pipes_log, events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", i);
			}
			
			
			
			int r = child_do_work(&balance_history, i , pipes_log, events_log);
			if(r!=0){
				exit(1);
			}
			
			
			//sleep(2);
			int r2 = done_work(&balance_history, i,pipes_log,events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", i);
			}
			
			
			exit(0);
			
		}
		
		}
		
		
	}
	
	
}
