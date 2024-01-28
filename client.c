#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 

#ifdef DEBUG
	#define debug(fn) fn
#else
	#define debug(fn)
#endif

#ifdef MAIN
int 
main(int argc, char *argv[]) 
{ 
	if(argc != 3){
		fprintf(stderr,"Usage: %s [ip_addr:port_num] [command]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in serv_addr; 
	int sock_fd ;
	int s, len ;
	char buffer[1024] = {0}; 
	char * data ;
	int port_num;
	char *ip_addr;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	ip_addr = strtok(argv[1],":");
	if(ip_addr == NULL){
		fprintf(stderr, "Invalid IP address\n Usage: %s [ip_addr:port_num] [command]\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	sscanf(strtok(NULL,":"),"%d",&port_num);
	printf("%d\n",port_num);
	if(port_num < 1024 || port_num > 49151){
		fprintf(stderr, "Invalid port number\n Usage: %s [ip_addr:port_num] [command]\n",argv[0]);
		exit(EXIT_FAILURE);
	}
	debug(printf("IP address: %s\n",ip_addr));
	debug(printf("Port Number: %d\n",port_num));	

	memset(&serv_addr, '\0', sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(port_num); 
	if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed : ") ; 
		exit(EXIT_FAILURE) ;
	} 

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	scanf("%s", buffer) ;
	
	data = buffer ;
	len = strlen(buffer) ;
	s = 0 ;
	while (len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
		data += s ;
		len -= s ;
	}

	shutdown(sock_fd, SHUT_WR) ;

	char buf[1024] ;
	data = 0x0 ;
	len = 0 ;
	while ( (s = recv(sock_fd, buf, 1023, 0)) > 0 ) {
		buf[s] = 0x0 ;
		if (data == 0x0) {
			data = strdup(buf) ;
			len = s ;
		}
		else {
			data = realloc(data, len + s + 1) ;
			strncpy(data + len, buf, s) ;
			data[len + s] = 0x0 ;
			len += s ;
		}

	}
	printf(">%s\n", data); 

} 
#endif