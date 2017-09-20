#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

const char *const port = "3490";
const int backlog = 10;

void sigchld_handle(int sig)
{
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sock;

	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	int yes = 1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	int tmp;
	if((tmp = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
		fprintf( stderr, "server: getaddrinfo: %s\n", gai_strerror(tmp));
		exit(EXIT_FAILURE);
	}

	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("socket()");
			continue;
		}
		
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1){
			perror("server: could not set socket for reuse.");
			exit(EXIT_FAILURE);
		}
		if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
			close(sock);
			perror("server: bind()");
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo);

	if(p == NULL){
		fprintf(stderr,"server: No sutable internet connection.\n");
		exit(EXIT_FAILURE);
	}
	if(listen(sock, backlog) == -1){
		perror("server: listen()");
		exit(EXIT_FAILURE);
	}

	struct sigaction sa;
	sa.sa_handler = sigchld_handle;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1){
		perror("server: sigaction()");
		exit(EXIT_FAILURE);
	}

	int conn;
	printf("server: waiting for connections.\n");
	while(1){
		sin_size = sizeof their_addr;
		conn = accept(sock, (struct sockaddr *)&their_addr, &sin_size);
		if(conn == -1){
			perror("server: accept()");
			continue;
		}
		char s[INET6_ADDRSTRLEN];
		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: connection from %s\n",s);
		if(!fork()){
			close(sock);
			char *string = "\
#include <stdio.h>\n\
int main(void){puts(\"Hello, World!\"); return 2;}\n";
			char to_send[256*2+1];
			snprintf(to_send,256*2,"%s",string);
			if(send(conn, to_send, strlen(to_send), 0) == -1)
				perror("server: send()");
			close(conn);
			exit(EXIT_SUCCESS);
		}
		close(conn);
	}
	return 0;
}





