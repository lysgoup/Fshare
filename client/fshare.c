#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <unistd.h>

#define BUF_SIZE 512

#ifdef DEBUG
	#define debug(fn) fn
#else
	#define debug(fn)
#endif

typedef enum {
    LIST,
    GET,
    PUT,
    N_OP
} commands;

char *op_str[N_OP] = {
	"list",
	"get",
	"put"
};

int recv_bytes(int sock, void *buf, size_t len){
	char *p = (char *)buf;
	int acc = 0;
	int received;
	while(acc < len){
		received = recv(sock, p, len-acc, 0);
		if(received == 0 || received == -1){
			return 1;
		}
		acc += received;
		p += received;
	}
	return 0;
}

int send_bytes(int sock, void *buf, size_t len){
	char *p = (char *)buf;
	int acc = 0;
	int sent;
	while(acc < len){
		sent = send(sock, p, len-acc, MSG_NOSIGNAL);
		if(sent == 0 || sent == -1){
			return 1;
		}
		acc += sent;
		p += sent;
	}
	return 0;
}

commands 
get_cmd(char *command)
{
	for(int i=0;i<N_OP;i++){
		if(strcmp(command,op_str[i])==0){
			return (commands) i;
		}
	}
	return N_OP;
}

void
recv_list(int sock_fd)
{
	size_t name_len;
	while(1){
		if(recv_bytes(sock_fd,(void *)&name_len,sizeof(name_len))) break;
		char *name = (char *)malloc(name_len+1);
		if(recv_bytes(sock_fd,(void *)name,name_len)){
			fprintf(stderr,"recv name error");
			free(name);
			return;
		}
		name[name_len] = '\0';
		printf("%s\n",name);
		free(name);
	}
}

void
download_file(int sock_fd, char *filename)
{
	send_bytes(sock_fd,(void *)filename,strlen(filename)+1);
	shutdown(sock_fd, SHUT_WR);

	long long filesize;
	recv_bytes(sock_fd,(void *)&filesize,sizeof(filesize));
	if(filesize == -1){
		fprintf(stderr,"There is no file named %s\n",filename);
		return;
	}
	debug(printf("filesize: %lld\n",filesize));

	FILE *fp = fopen(filename,"wb");
	char buf[BUF_SIZE];
	int received;
	long long count = 0;
	while(count < filesize){
		received = recv(sock_fd,buf,sizeof(buf),0);
		if(received <= 0) break;
		fwrite(buf,1,received,fp);
		count += received;
	}
	fclose(fp);
	printf("[%d] %s download\n",sock_fd,filename);
}

void 
upload_file(int sock_fd, char *filename)
{
	FILE *fp = fopen(filename,"rb");
	if(fp == NULL){
		perror("fopen error");
		return;
	}
	size_t filename_len = strlen(filename);
	send_bytes(sock_fd,(void *)&filename_len,sizeof(filename_len));
	send_bytes(sock_fd,(void *)filename,filename_len);
	char buf[BUF_SIZE];
	size_t bytesRead;
	while((bytesRead = fread(buf,1,sizeof(buf),fp)) > 0){
		send_bytes(sock_fd,(void *)buf,bytesRead);
	}
	shutdown(sock_fd,SHUT_WR);
	fclose(fp);
	printf("[%d] %s uploaded\n",sock_fd,filename);
}

#ifdef MAIN
int 
main(int argc, char *argv[]) 
{ 
	if(argc < 3){
		fprintf(stderr,"Usage: %s [ip_addr:port_num] [command]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in serv_addr; 
	int sock_fd ;
	char * data ;
	int port_num;
	char *ip_addr;
	commands cmd;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed") ;
		exit(EXIT_FAILURE) ;
	}

	ip_addr = strtok(argv[1],":");
	if(ip_addr == NULL){
		fprintf(stderr, "Invalid IP address\n Usage: %s [ip_addr:port_num] [command]\n",argv[0]);
		exit(EXIT_FAILURE);
	}

	sscanf(strtok(NULL,":"),"%d",&port_num);
	if(port_num < 1024 || port_num > 49151){
		fprintf(stderr, "Invalid port number\n Usage: %s [ip_addr:port_num] [command]\n",argv[0]);
		return 0;
	}
	debug(printf("IP address: %s\n",ip_addr));
	debug(printf("Port Number: %d\n",port_num));
	debug(printf("User Command: %s\n",argv[2]));

	cmd = get_cmd(argv[2]);
	if(cmd == N_OP){
		fprintf(stderr, "Invalid command\n");
		return 0;
	}
	debug(printf("cmd: %d\n",cmd);)

	memset(&serv_addr, '\0', sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(port_num); 
	if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed") ; 
		return 0;
	} 

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed") ;
		return 0;
	}
	
	if(send_bytes(sock_fd,(void *)&cmd,sizeof(cmd)) == 1){
		perror("send_bytes error");
		return 0;
	}
	
	switch(cmd){
		case LIST:
			recv_list(sock_fd);
			break;
		case GET:
			if(argc < 4) fprintf(stderr,"No file name\n");
			else download_file(sock_fd, argv[3]);
			break;
		case PUT:
			if(argc < 4) fprintf(stderr,"No file name\n");
			else upload_file(sock_fd, argv[3]);
	}
	close(sock_fd);
} 
#endif
