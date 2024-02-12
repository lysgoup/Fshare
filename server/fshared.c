// Partly taken from https://www.geeksforgeeks.org/socket-programming-cc/

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>

#define FILE_NAME_SIZE 256
#define FILE_PATH_SIZE 512
#define BUF_SIZE 512

#ifdef DEBUG
	#define debug(fn) fn
#else
	#define debug(fn)
#endif

typedef enum{
	LIST,
	GET,
	PUT,
	N_OP
}commands;

char *dir_path;
DIR *dir;
pthread_mutex_t dir_lock = PTHREAD_MUTEX_INITIALIZER;

void 
signal_handler(int sig)
{
	if(sig == SIGINT){
		printf("close dir\n");
		closedir(dir);
		printf("exit success\n");
		exit(EXIT_SUCCESS);
	}
}

int 
recv_bytes(int sock, void *buf, size_t len)
{
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

int 
send_bytes(int sock, void *buf, size_t len)
{
	char *p = (char *)buf;
	int acc = 0;
	int sent;
	while(acc < len){
		sent = send(sock, p, len-acc, 0);
		if(sent == 0 || sent == -1){
			return 1;
		}
		acc += sent;
		p += sent;
	}
	return 0;
}

void
send_list_subdir(char *subdir_path, int sock_fd)
{
	debug(printf("This is send_list_subdir\n");) 
	DIR *subdir = opendir(subdir_path);	
	if(subdir == NULL){
		fprintf(stderr,"Cannot open dir %s\n",subdir_path);
		return;
	}
	struct dirent *entry;
	while((entry = readdir(subdir)) != NULL){
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		size_t file_path_size = strlen(subdir_path)+strlen(entry->d_name)+2;
		char *file_path = (char *)malloc(file_path_size);
		snprintf(file_path,file_path_size,"%s/%s",subdir_path,entry->d_name);
		char *name = file_path + strlen(dir_path) + 1;
		size_t name_len = strlen(name);
		if(send_bytes(sock_fd,(void *)&name_len,sizeof(name_len))){
			fprintf(stderr,"send name_len error\n");
			return;
		}
		if(send_bytes(sock_fd,(void *)name,strlen(name))){
			fprintf(stderr,"send name error\n");
			return;
		}
		debug(printf("%s : %ld\n",name,strlen(name));)
		struct stat filestat;
		stat(file_path,&filestat);
		if(S_ISDIR(filestat.st_mode)){
			send_list_subdir(file_path,sock_fd);
		}
		free(file_path);
	}
	closedir(subdir);	
}

void
send_list(int sock_fd)
{
	pthread_mutex_lock(&dir_lock);
	struct dirent *entry;
	rewinddir(dir);
	while((entry = readdir(dir)) != NULL){
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		char *name = entry->d_name;
		size_t name_len = strlen(name);
		if(send_bytes(sock_fd,(void *)&name_len,sizeof(name_len))){
			fprintf(stderr,"send name_len error\n");
			return;
		}
		if(send_bytes(sock_fd,(void *)name,strlen(name))){
			fprintf(stderr,"send name error\n");
			return;
		}
		debug(printf("%s : %ld\n",name,strlen(name));)
		char *file_path = (char *)malloc(strlen(dir_path)+name_len+1);
		snprintf(file_path,strlen(dir_path)+name_len+2,"%s/%s",dir_path,name);
		struct stat filestat;
		stat(file_path,&filestat);
		if(S_ISDIR(filestat.st_mode)){
			send_list_subdir(file_path,sock_fd);
		}
		free(file_path);
	}
	shutdown(sock_fd,SHUT_WR);
	pthread_mutex_unlock(&dir_lock);
	printf("[%d] send list success\n",sock_fd);
}

void 
send_file(int sock_fd)
{
	char filename[FILE_NAME_SIZE];
	recv_bytes(sock_fd,(void *)filename,sizeof(filename));
	pthread_mutex_lock(&dir_lock);
	char filepath[FILE_PATH_SIZE];
	struct stat filestat;
	snprintf(filepath,sizeof(filepath),"%s/%s",dir_path,filename);
	debug(printf("sending %s...\n",filepath));

	long long filesize;
	if(stat(filepath,&filestat) == 0){
		if (S_ISREG(filestat.st_mode)){
			filesize = filestat.st_size;
		}
		else filesize = -1;
	}
	else{
		perror("stat error");
		filesize = -1;
	}
	
	// send filesize
	debug(printf("filesize: %lld\n",filesize);)
	send_bytes(sock_fd,(void *)&filesize,sizeof(filesize));
	if(filesize == -1){
		pthread_mutex_unlock(&dir_lock);
		return;
	}

	//send file
	FILE *fp = fopen(filepath,"rb");
	char buf[BUF_SIZE];
	size_t bytesRead;
	while((bytesRead = fread(buf,1,sizeof(buf),fp)) > 0){
		send_bytes(sock_fd,(void *)buf,bytesRead);
	}
	fclose(fp);
	pthread_mutex_unlock(&dir_lock);
	printf("[%d] send %s success\n",sock_fd,filepath);
}

void 
recv_file(int sock_fd)
{
	size_t filename_len;
	if(recv_bytes(sock_fd,(void *)&filename_len,sizeof(filename_len))){
		fprintf(stderr,"failed to receive file\n");
		return;
	}
	char *filename = (char *)malloc(filename_len+1);
	recv_bytes(sock_fd,(void *)filename,filename_len);
	filename[filename_len] = '\0';

	debug(printf("filename: %s\n",filename);)
		
	char *temp=NULL, *token;
	if(strtok(filename,"/")!=NULL){
		while((token=strtok(NULL,"/"))!=NULL){
				free(temp);
				temp = strdup(token);
		}
		free(filename);
		filename = temp;
	}
	
	char filepath[FILE_PATH_SIZE];
	snprintf(filepath,sizeof(filepath),"%s/%s",dir_path,filename);
	debug(printf("filepath: %s\n",filepath);)

	pthread_mutex_lock(&dir_lock);
	int error_check;
	FILE *fp = fopen(filepath,"wb");
	if(fp==NULL){
		perror("Cannot make file");
		error_check=1;
	}
	else{
		char buf[BUF_SIZE];
		int received;
		long long count = 0;
		while((received= recv(sock_fd,buf,sizeof(buf),0)) > 0){
			fwrite(buf,1,received,fp);
		}
		fclose(fp);
		printf("[%d] reveive %s success\n",sock_fd,filepath);
	}
	pthread_mutex_unlock(&dir_lock);
	send_bytes(sock_fd,&error_check,sizeof(error_check));
	debug(printf("error_check: %d\n",error_check);)
}	


void *
handle_client(void *sock)
{
  int conn = *((int *)sock);
	int cmd;
	char buf[1024] = {0};

	if(recv_bytes(conn, (void *)&cmd, sizeof(cmd))){
		fprintf(stderr,"recv cmd error\n");
		close(conn);
		return NULL;
	}
	debug(printf("cmd: %d\n", cmd);)

	switch(cmd){
		case LIST:
			send_list(conn);
			break;
		case GET:
			send_file(conn);
			break;
		case PUT:
			recv_file(conn);
	}
	close(conn);
}

#ifdef MAIN
int 
main(int argc, char *argv[])
{
	int opt;
	int port_num;

	//Parsing command line arguments
	if(argc < 5){
		fprintf(stderr, "Usage: %s [-p port_num] [-d dir_path]\n", argv[0]);
        return 1;	
	}
	while((opt = getopt(argc, argv, "p:d:")) != -1){
		switch(opt){
			case 'p':
				sscanf(optarg, "%d\n", &port_num);
				if(port_num < 1024 || port_num > 49151){
					fprintf(stderr,"Invalid port number");
					exit(EXIT_FAILURE);
				}
				break;
			case 'd':
				dir_path = (char *)malloc(strlen(optarg));
				memcpy(dir_path, optarg, strlen(optarg));
				break;
			default:
				fprintf(stderr, "Usage: %s [-p port_num] [-d dir_path]\n", argv[0]);
				return 1;
		}
	}
	debug(printf("port num: %d\n",port_num));
	debug(printf("dir path: %s\n",dir_path));
	dir = opendir(dir_path);
	if(dir == NULL){
		perror("Cannot open directory");
		exit(EXIT_FAILURE);
	}

	signal(SIGINT, signal_handler);

  pthread_t tid;
	int listen_fd, new_socket ; 
	struct sockaddr_in address; 
	int addrlen = sizeof(address); 

	//Make connection
	listen_fd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0 /*IP*/) ;
	if (listen_fd == 0)  { 
		perror("socket failed : "); 
		exit(EXIT_FAILURE); 
	}
	
	memset(&address, '0', sizeof(address)); 
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = INADDR_ANY /* the localhost*/ ; 
	address.sin_port = htons(port_num); 
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : "); 
		exit(EXIT_FAILURE); 
	} 

	while (1) {
		if (listen(listen_fd, 16 /* the size of waiting queue*/) < 0) { 
			perror("listen failed : "); 
			exit(EXIT_FAILURE); 
		} 

		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if (new_socket < 0) {
			perror("accept"); 
			exit(EXIT_FAILURE); 
		}

		if(pthread_create(&tid,NULL,handle_client,&new_socket)!=0){
				perror("pthread create error");
		}
	}
} 
#endif
