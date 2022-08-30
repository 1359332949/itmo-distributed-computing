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

int receive_any(void *self , Message *msg){
	int i;
    for(i = 0 ; i <= pro_num ; i++){
        if(receive(self, i, msg) == 0)
            return -1;
    }
    return i;
}