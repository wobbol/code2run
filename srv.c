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
#include <sys/stat.h>
#include <signal.h>
#include <stdarg.h>

const char *const port = "3490";
const int backlog = 10;
const int max_data_size = 100;

void rmtmp(void)
{
	if(!remove("/dev/shm/code2run/code"))
		perror("could not remove source code tmp file");
	if(!rmdir("/dev/shm/code2run"))
		perror("could not remove tmpdir");
}

void fail(const char *const fmt, ...){
	fprintf(stderr,"server: ");

	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	rmtmp();
	exit(EXIT_FAILURE);
}

void sigchld_handle(int sig)
{
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void install_sighandler(void)
{
	struct sigaction sa;
	sa.sa_handler = sigchld_handle;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1)
		fail("sigaction()");
}

void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void init_sockfd(struct addrinfo *servinfo, int *sockfd)
{ /* setup sockfd */
	int yes = 1;
	struct addrinfo *p;
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("server: socket()");
			continue;
		}

		if(setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
			fail("could not set socket for reuse.");

		if(bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(*sockfd);
			perror("server: bind()");
			continue;
		}
		break;
	}

	freeaddrinfo(servinfo);

	if(p == NULL)
		fail("No sutable internet connection.");
}

void get_bits(int conn, char *buf, char *filename)
{ /* gets bits */
	int numbytes;
	if ((numbytes = recv(conn, buf, max_data_size-1, 0)) == -1)
		fail("recv");

	buf[numbytes] = '\0';

	FILE *code = fopen(filename, "w+");
	if(!code)
		perror("get_bits");
	fprintf(code, "%s", buf);
	fclose(code);
}

void compile(char *infile, char *outfile, int cleanup)
{ /* compile */
	int status;
	pid_t pid;

	pid = fork();
	if(pid == 0){
		char * gcc_arg[] = {"gcc", "-x", "c", infile, "-o", outfile, (char*)0};
		execvp(gcc_arg[0], gcc_arg);
	}
	wait(&status);
	if(cleanup)
		remove(infile);
}

int run(char *name, int cleanup, int *status)
{ /* run */
	pid_t pid;
	int filedes[2];
	// 0 read
	// 1 write
	if(pipe(filedes))
		perror("No pipe");

	pid = fork();
	if(pid == 0){
		close(filedes[0]);
		dup2(filedes[1],1);
		char *danger_arg[] = {name, (char*)0};
		execvp(danger_arg[0], danger_arg);
	}

	close(filedes[1]);
	wait(status);
	if(cleanup)
		remove(name);
	return filedes[0];
}

int main(void)
{
	const char *const tmpdir = "/dev/shm/code2run";
	if(mkdir(tmpdir, 0755))
		fail("could not make temp dir.");

	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	int sockfd;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	int yes = 1;

	struct addrinfo *servinfo;

	{
		int tmp;
		if((tmp = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
			fail("server: getaddrinfo: %s\n", gai_strerror(tmp));
		}
	}
	init_sockfd(servinfo, &sockfd);

	if(listen(sockfd, backlog) == -1)
		fail("listen()");

	install_sighandler();

	puts("server: waiting for connections.");
	int conn;
	while(1){
		sin_size = sizeof their_addr;
		conn = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(conn == -1){
			perror("server: accept()");
			continue;
		}
		char s[INET6_ADDRSTRLEN];
		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: connection from %s\n", s);
		if(!fork()){
			close(sockfd);

			char buf[max_data_size];

			get_bits(conn, buf, "/dev/shm/code2run/code");

			pid_t pid;
			int status;

			compile("/dev/shm/code2run/code", "totally_unique", 1);
			int fd = run("./totally_unique", 1, &status);

			char ch[256];
			int re;
			while((re = read(fd,ch,256)) > 0){
				if(errno)
					break;
			if(send(conn, ch, re, 0) == -1)
				perror("server: send()");
			}

			char exitstatus[6] = {0};
			int el;
			el = snprintf(exitstatus, 6, "%d", WEXITSTATUS(status));
			if(send(conn, exitstatus, el, 0) == -1)
				perror("server: send()");

			close(conn);
			exit(EXIT_SUCCESS);
		}
		close(conn);
	}
	return 0;
}
