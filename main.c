#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strerror for now */
#include <errno.h>
#include <sys/socket.h>

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
	char *gcc_arg[] = {"gcc", "-x","c","-", "-o", "totally_unique", (char*)0};
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


int main(int argc, char **argv)
{
	const int buf_size = 256;
//int sock = socket(AF_INET, SOCK_STREAM,0);

	{ /*TODO: refactor*/
		//TODO get input from socket connection using
		// %%super_specal_seperator%% as an end data mark
		int cat_pipe[2];
		pipe(cat_pipe);

		pid_t cat = cmd(run_gcc_c, STDIN_FILENO, cat_pipe[write_end]);
		if(cat == -1)
			fatal(argv[0], "Fork (edit this if strerror prints .*fork.*)");

		close(cat_pipe[write_end]);

		char buf[buf_size];
		while(0 != read(cat_pipe[read_end],buf,buf_size)){
			/*empty pipe so cmd can exit*/ 
			//printf("%s",buf);
		}
	}

	{ /*TODO: refactor*/
		int danger_pipe[2];
		pipe(danger_pipe);

		pid_t danger = cmd(run_gcc_c, STDIN_FILENO, danger_pipe[write_end]);
		if(danger == -1)
			fatal(argv[0], "Fork (edit this if strerror prints .*fork.*)");

		close(danger_pipe[write_end]);

		char buf[buf_size];
		while(0 != read(danger_pipe[read_end],buf,buf_size)){
			/*danger wil not exit till this is finished*/ 
			printf("%s",buf);
		}
	}
	

	return 0;
}
