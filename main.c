#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strerror for now */
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>

enum command{
	run_danger,
	run_cat,
	run_gcc_c,
	run_gcc_cxx
};

enum learn_pipe{
	read_end = 0,
	write_end = 1
};

/* returns pid on success and -1 on fail. Sets errno */
int cmd(enum command cmd, int fdin, int fdout)
{
	pid_t pid = -1;

	char *cat_arg[] = {"cat", (char*)0};
	char *danger_arg[] = {"./totally_unique", (char*)0};
	char *gcc_arg[] = {"gcc", "-x","c","/tmp/code2run", "-o", "totally_unique", (char*)0};
	char **arg_list;

	switch(cmd)
	{
	case run_cat:	
		/* straight through for dev */
		arg_list = cat_arg;
		break;
	case run_gcc_c:	
		arg_list = gcc_arg;
		break;
	case run_gcc_cxx:	
		arg_list = gcc_arg;
		break;
	case run_danger:	
		arg_list = danger_arg;
		break;
	default:
		return pid;
	}


	pid = fork();
	if(pid == 0){
		/* Child process. */
		dup2(fdin,STDIN_FILENO);
		dup2(fdout,STDOUT_FILENO);

		execvp(arg_list[0], arg_list);

		perror("Failed to exec.");
		fprintf(stdout,"Exec unavalable.");
		return 1;
	}
	return pid;
}

void fatal(const char *const name, const char *const str)
{
	fprintf(stderr,"%s: %s", strerror(errno), str);
	fprintf(stderr,"%s: Do'h!", name);
	exit(EXIT_FAILURE);
}

void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv)
{
	const int buf_size = 256*2;
	const char *url = "127.0.0.1";

	struct addrinfo *res;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);

	/* use ipv4/6 tcp and use my ip address. */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(url, "3490", &hints, &res);
	if(status != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}
	struct addrinfo *i;
	int sock;
	for(i = res; i != NULL; i = i->ai_next){
		sock = socket(
		i->ai_family,
		i->ai_socktype,
		i->ai_protocol
		);

		if(sock == -1){
			perror("socket");
			continue;
		}
		if(connect(sock, i->ai_addr, i->ai_addrlen) == -1){
			close(sock);
			perror("connect");
			continue;
		}
		break;
	}

	if(i == NULL){
		fprintf(stderr,"failed to connect to %s\n",url);
		return EXIT_FAILURE;
	}

	int bin_len = sizeof(struct in6_addr);
	char str_addr[INET6_ADDRSTRLEN];
	char bin_addr[sizeof(struct in6_addr)];

	memcpy(
	bin_addr,
	get_in_addr((struct sockaddr *)i->ai_addr),
	bin_len
	);

	inet_ntop(
	i->ai_family,
	bin_addr,
	str_addr,
	sizeof str_addr
	);

	printf("connecting.\n");
	freeaddrinfo(res);

	int numbytes;
	char buf[buf_size];
	numbytes = recv(sock,buf,buf_size-1,0);
	if(numbytes == -1){
		perror("recv");
		return EXIT_FAILURE;
	}
	buf[numbytes] = '\0';

	FILE *code = fopen("/tmp/code2run","w+");
	fprintf(code,"%s",buf);
	fclose(code);

	{ /*TODO: refactor*/
		//TODO get input from socket connection using
		// %%super_specal_seperator%% as an end data mark
		int cat_pipe[2];
		pipe(cat_pipe);

		pid_t cat;

		cat = cmd(
		run_gcc_c,
		STDIN_FILENO,
		STDOUT_FILENO
		);

		if(cat == -1)
			fatal(argv[0], "Fork (edit this if strerror prints .*fork.*)");

		close(cat_pipe[write_end]);

		int status;
		wait(&status);
	}

	{ /*TODO: refactor*/
		int danger_pipe[2];
		pipe(danger_pipe);

		pid_t danger = cmd(run_danger, STDIN_FILENO, danger_pipe[write_end]);
		if(danger == -1)
			fatal(argv[0], "Fork (edit this if strerror prints .*fork.*)");

		close(danger_pipe[write_end]);

		char out_buf[buf_size];
		while(0 != read(danger_pipe[read_end],out_buf,buf_size)){
			/*danger wil not exit till this is finished*/ 
			printf("%s",out_buf);
		}
	}
	return 0;
}
