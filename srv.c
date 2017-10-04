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
			perror("server: socket()");
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

const int max_data_size = 100;
char buf[max_data_size];
			{ /* gets bits */
				int numbytes;
				if ((numbytes = recv(conn, buf, max_data_size-1, 0)) == -1) {
					perror("server: recv");
					exit(1);

					buf[numbytes] = '\0';
				}

				FILE *code = fopen("/dev/shm/code2run/code","w+");
				fprintf(code,"%s",buf);
				fclose(code);
			}
				pid_t pid;
				int status;
			{ /* compile */
				pid = fork();
				if(pid == 0){
					char *gcc_arg[] = {"gcc", "-x","c","/dev/shm/code2run/code", "-o", "totally_unique", (char*)0};
					execvp(gcc_arg[0],gcc_arg);
				}
				wait(&status);
				//remove("/dev/shm/code2run/code");
			}
			{ /* run */

				pid = fork();
				if(pid == 0){
					char *danger_arg[] = {"./totally_unique", (char*)0};
					execvp(danger_arg[0],danger_arg);
					//remove("./totally_unique");
				}
			}

			wait(&status);
			char to_send[20];
			snprintf(to_send,20,"%d",WEXITSTATUS(status));
			if(send(conn, to_send, 20, 0) == -1)
				perror("server: send()");
			
			close(conn);
			exit(EXIT_SUCCESS);
		}
		close(conn);
	}
	return 0;
}





