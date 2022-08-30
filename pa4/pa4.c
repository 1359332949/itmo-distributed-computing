#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include<errno.h>
#include "ipc.h"

#include "pa2345.h"

#define PROCESS_MAX 10



int pro_num;
int childpid[PROCESS_MAX+1];
int fd[PROCESS_MAX+1][PROCESS_MAX+1][2];

static timestamp_t lamport_time = 0;

struct Node{
	struct Node* prev;	
	struct Node* next;	
	int key;			
	int value;			
};

typedef struct Node QueueNode;

typedef struct{
	QueueNode* head;	
	QueueNode* tail;	
} LamportQueue;
typedef struct{
	local_id pid;
	LamportQueue* queue;
	int left;
	
} Commu;
int node_cmp(QueueNode* one, QueueNode* two){
	if (one->key < two->key){
		return -1;
	}
	if (one->key > two->key){
		return 1;
	}
    if (one->value < two->value){
		return -1;
	}
    if (one->value > two->value){
		return 1;
	}
	return 0;
}

/*Init Lamport Queue*/
LamportQueue* lamport_queue_init(){
	LamportQueue* queue = malloc(sizeof(LamportQueue));
	queue->head = NULL;
	queue->tail = NULL;
	return queue;
}


void lamport_queue_destroy(LamportQueue* queue){
	if (queue->head != NULL){
		QueueNode* node = queue->head;
		do{
			QueueNode* next = node->next;
			free(node);
			node = next;
		} while(node != NULL);
	}
	free(queue);
}


void lamport_queue_insert(LamportQueue* queue, timestamp_t key, local_id value){
	QueueNode* node = malloc(sizeof(QueueNode));
	node->key = key;
	node->value = value;
	node->prev = NULL;
	node->next = NULL;
	
	if (queue->head == NULL){
		queue->head = node;
		queue->tail = node;
	}
	
	else if (node_cmp(node, queue->head) < 0){
		node->next = queue->head;
		queue->head->prev = node;
		queue->head = node;
	}
	
	else if (node_cmp(node, queue->tail) > 0){
		node->prev = queue->tail;
		queue->tail->next = node;
		queue->tail = node;
	}
	else{
		QueueNode* tmp_left = queue->head;		
		QueueNode* tmp_right = queue->head->next;
		
		while(!(node_cmp(node, tmp_left) > 0 && node_cmp(node, tmp_right) < 0)){
			tmp_left = tmp_right;
			tmp_right = tmp_left->next;
		}
		
		node->prev = tmp_left;
		node->next = tmp_right;
		tmp_left->next = node;
		tmp_right->prev = node;
	}	
}


local_id lamport_queue_peek(LamportQueue* queue){
	if (queue->head == NULL){
		return -1;
	}
	return queue->head->value;
}


local_id lamport_queue_get(LamportQueue* queue){
	QueueNode* tmp_head;
	local_id retval;
	if ((retval = lamport_queue_peek(queue)) < 0){
		return -1;
	}
	
	tmp_head = queue->head->next;
	if (tmp_head != NULL){
		tmp_head->prev = NULL;
	}
	
	free(queue->head);
	queue->head = tmp_head;
	return retval;
}

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
	
    int read_result = read(fd1, msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    if (read_result < 0) {
		if(read_result == -1 && errno == EAGAIN)
			return -1;
		//perror("read");
    }
    return read_result > 0 ? 0 : -1;
}
int send_multicast(void * self, const Message * msg) {
	for (local_id i = 0; i <= pro_num; i++) {
        if(i != *(local_id*)self){
			send(self, i, msg);
			//fprintf(stdout, "%d send msg len=%d type=%d to %d \n", *(local_id*)self, msg->s_header.s_payload_len, msg->s_header.s_type, i);
			
			
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


int done_work(local_id pid, FILE *pipes_log, FILE *events_log){
	
	
	if(pid!=0){
		add_lamport_time();
		Message msg;
		msg.s_header.s_magic = MESSAGE_MAGIC;
		msg.s_header.s_payload_len = 0;
		msg.s_header.s_type = DONE;
		msg.s_header.s_local_time = get_lamport_time();
		send_multicast(&pid, &msg);
		//fprintf(stdout,"%d send done=%d to every\n",pid, msg.s_header.s_type);
		add_lamport_time();
		
	}
	Message msg1;
	msg1.s_header.s_magic = MESSAGE_MAGIC;
    msg1.s_header.s_type = DONE;
	msg1.s_header.s_payload_len = 0;
	for (local_id i = 1; i <= pro_num; i++) {
		
       if (i != pid&&pid!=0){
		   
		   while(receive(&pid, i, &msg1) < 0);
		   //fprintf(stdout,"%d receive done %d from:%d, len = %lu\n",pid, msg1.s_header.s_type, i, sizeof(MessageHeader) + msg1.s_header.s_payload_len);
	   }
	   else if(pid==0){
		   //sleep(4);
		   
		   while(receive(&pid, i, &msg1) < 0);
		   //fprintf(stdout,"%d receive done %d from:%d\n",pid, msg1.s_header.s_type, i);
	   }
           
	
    }
	set_lamport_time_from_msg(&msg1);
	fprintf(events_log, log_received_all_done_fmt,get_lamport_time(), pid);
	
	return 0;
	
}
int start_init(local_id pid, FILE *pipes_log, FILE *events_log){
	
	close_pipe(pid, pipes_log, fd);
	
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = 0;
    msg.s_header.s_type = STARTED;
	msg.s_header.s_local_time = get_lamport_time();
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
int cs_work(Commu* cm, Message* msg){
	local_id pid = cm->pid;
	LamportQueue* queue = cm->queue;
	if (msg->s_header.s_type == CS_REQUEST){
		//fprintf(stdout,"%d received request from %d \n", pid ,*(local_id*)(msg->s_payload));
        lamport_queue_insert(queue, msg->s_header.s_local_time-1, *(local_id*)(msg->s_payload));
		set_lamport_time_from_msg(msg);
		add_lamport_time();
		Message msg1;
		msg1.s_header.s_magic = MESSAGE_MAGIC;

		msg1.s_header.s_payload_len = sizeof(local_id);
		msg1.s_header.s_type = CS_REPLY;
		msg1.s_header.s_local_time = get_lamport_time();
		memcpy(msg1.s_payload, &pid, sizeof(local_id));
        while(send(&pid, *(local_id*)(msg->s_payload), &msg1)<0);
		//fprintf(stdout,"%d send reply to %d len = %lu\n", pid ,*(local_id*)(msg->s_payload), sizeof(MessageHeader) + msg1.s_header.s_payload_len);
    }
    else if (msg->s_header.s_type == CS_RELEASE){
        if (lamport_queue_get(queue) != *(local_id*)msg->s_payload){
			set_lamport_time_from_msg(msg);
            return -1;
        }
    }
	else if (msg->s_header.s_type == DONE){
		set_lamport_time_from_msg(msg);
		cm->left--;
	}
	
	
	return 0;
}
int request_cs(const void * self){
	
	Commu* cm = (Commu*) self;
	local_id pid = cm->pid;
	LamportQueue* queue = cm->queue;
	

	lamport_queue_insert(queue, get_lamport_time(), pid);
	add_lamport_time();
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;

	msg.s_header.s_payload_len = sizeof(local_id);
	msg.s_header.s_type = CS_REQUEST;
	msg.s_header.s_local_time = get_lamport_time();
	memcpy(msg.s_payload, &pid, sizeof(local_id));
	
	for (local_id i = 1; i <= pro_num; i++) {
        if(i != *(local_id*)self){
			while(send((Commu*)self, i, &msg)<0);
			//fprintf(stdout, "%d send request to %d\n", pid, i);
		}
			
		
    }
	add_lamport_time();
	int reply_left = pro_num - 1;
	Message msg1;
	msg1.s_header.s_payload_len = sizeof(local_id);
	while(reply_left){
		
		while((receive_any(&pid, &msg1))<0);
		
		if(msg1.s_header.s_type == CS_REPLY){
			set_lamport_time_from_msg(&msg1);
			reply_left--;
			
			//fprintf(stdout, "%d received reply from %d, remain %d len = %lu \n", pid, *(local_id*)msg1.s_payload, reply_left, sizeof(MessageHeader) + msg1.s_header.s_payload_len);
		}else{
			
			cs_work((Commu*)self, &msg1);
		}
		
		
		
			
			
	}		
	
	
	Message msg2;
	msg2.s_header.s_payload_len = sizeof(local_id);
	while (lamport_queue_peek(queue) != pid){
		//printf("---+%d++++++++++++++-----%d----------\n", pid, lamport_queue_peek(queue));
		
		while (receive_any(&pid, &msg2)<0);
			
		set_lamport_time_from_msg(&msg2);
		
		cs_work((Commu*)self, &msg2);
	}
	
	return 0;
}
int release_cs(const void * self){
	Commu* cm = (Commu*) self;
	local_id pid = cm->pid;
	LamportQueue* queue = cm->queue;
	add_lamport_time();
	Message msg;
	msg.s_header.s_magic = MESSAGE_MAGIC;
	msg.s_header.s_type = CS_RELEASE;
	msg.s_header.s_local_time = get_lamport_time();
	msg.s_header.s_payload_len = sizeof(local_id);
	memcpy(msg.s_payload, &pid, sizeof(local_id));
	for (local_id i = 1; i <= pro_num; i++) {
       if (i != pid)
           while(send(&pid, i, &msg) < 0);
			
    }
	add_lamport_time();
	
	lamport_queue_get(queue);
	return 0;
}


int child_do_work(Commu *cm, int mutexl, FILE *pipes_log, FILE *events_log){
		local_id pid = cm->pid;
		char buf[256];
		
		
		for (int i = 1; i <= pid * 5; i++){
		
		if (mutexl){
			
			request_cs(cm);
		}
		
		/* Critical area */
		
		snprintf(buf, 256, log_loop_operation_fmt, pid, i, pid * 5);
		
		print(buf);
		
		if (mutexl){
			release_cs(cm);
		}
		}
		
		
		
	
		
		
	
	
	
	
		
		
		
		
		
	
	return 0;
}
int get_agrs(int argc, char** argv, int* processes, int* mutexl){
	int res;
	const struct option long_options[] = {
        {"mutexl", no_argument, mutexl, 1},
        {NULL, 0, NULL, 0}
    };
	
	*mutexl = 0;
	
	while ((res = getopt_long(argc, argv, "p:", long_options, NULL)) != -1){
		
		if (res == 'p'){
			
			
			*processes = atoi(optarg);
		}
		else if (res == '?'){
			return -1;
		}
		
	}
	return 0;
}
int main(int argc, char **argv){
	FILE *pipes_log = NULL;
	FILE *events_log = NULL;
	
	int mutexl;
	if (argc < 3 || get_agrs(argc, argv, &pro_num, &mutexl) == -1){
			fprintf(stderr, "Usage:%s -p X [--mutexl]\n", argv[0]);
			return -1;
	}
	//printf("----%d-----\n",pro_num);
	
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
		
		int r1 = start_init(0, pipes_log, events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", 0);
			}
		
		
		
		
	
		int r2 = done_work(0, pipes_log, events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", 0);
			}
		
		
		
			
		int child_num = pro_num;
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
			LamportQueue* queue = lamport_queue_init();
			
			Commu cm=(Commu){
				.pid = i,
				.queue = queue,
				.left = pro_num-1
			};
			int r1 = start_init(i, pipes_log, events_log);
			if(r1!=0){
				fprintf(events_log,"Process %d creat failed.\n", i);
			}
			
			int r = child_do_work(&cm, mutexl, pipes_log, events_log);
			if(r!=0){
				exit(1);
			}
			
				add_lamport_time();
				Message msg;
				msg.s_header.s_magic = MESSAGE_MAGIC;
				msg.s_header.s_payload_len = sizeof(local_id);
				memcpy(msg.s_payload, &i, sizeof(local_id));
				msg.s_header.s_type = DONE;
				msg.s_header.s_local_time = get_lamport_time();
				
				send_multicast(&i, &msg);
				//fprintf(stdout,"%d send done=%d to every-------------------------------------\n",i, msg.s_header.s_type);
				add_lamport_time();
				
			while (cm.left){
				Message message;
				message.s_header.s_payload_len = sizeof(local_id);
				
				while (receive_any(&i, &message)<0);
				
				cs_work(&cm, &message);
			}
			
			lamport_queue_destroy(queue);
			
			
		/**
		int r2 = done_work(i,pipes_log,events_log);
			if(r2!=0){
				fprintf(events_log,"Process %d done failed.\n", i);
			}
		
		**/
			
			
			
			exit(0);
			
		}
		
		}
		
		
	}
	
	
}
