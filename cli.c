#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <arpa/inet.h>

const char *const port = "3490"; // the port client will be connecting to 
const int MAX_DATA_LEN = 100; // TODO: only used for recv buf display all data from the connection in
const char *arg0;

void write_zero(char *str, int len)
{
	for(int i = 0; i < len; ++i)
		str[i] = 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// loop through all the results and connect to the first we can
void setup_sock(struct addrinfo *p, int *sockfd)
{
	for(; p != NULL; p = p->ai_next) {
		if (-1 == (*sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol))) {
			perror("client: socket");
			continue;
		}
		if (-1 == connect(*sockfd, p->ai_addr, p->ai_addrlen)) {
			close(*sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(2);
	}
	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
}

void setup_addrinfo(struct addrinfo **servinfo, struct addrinfo *hints, const char *const ip)
{
    int rv;
    if ((rv = getaddrinfo(ip, port, hints, servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
}

void recv_print(int fd)
{
	char buf[MAX_DATA_LEN];
	int bytes;
	while(1){
		bytes = recv(fd, buf, MAX_DATA_LEN, 0);
		if(bytes <= 0)
			break;
		printf("%.*s", bytes, buf);
	}
	if(bytes == -1)
		perror("recv final:");
}
void usage(void)
{
	fprintf(stderr,"usage: %s: hostname\n", argv0);
	exit(1);
}

int main(int argc, char *argv[])
{
    arg0 = argv[0];
    if (argc != 2) {
	    usage();
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *servinfo;
    setup_addrinfo(&servinfo, &hints, argv[1]);

    int sockfd;  

    setup_sock(servinfo, &sockfd);
    freeaddrinfo(servinfo); // all done with this structure
 

    char buf[MAX_DATA_LEN];
    char *hello = "#include <stdio.h>\nint main(void){puts(\"Hello, World!\"); return 2;}\n";

    snprintf(buf, MAX_DATA_LEN, "%s", hello);
    if(send(sockfd, buf, strlen(buf), 0) == -1)
	    perror("client: send()");

    puts("client: receiving");
    recv_print(sockfd);


    close(sockfd);
    exit(EXIT_SUCCESS);

    // end here

    close(sockfd);

    return 0;
}
