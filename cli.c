#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>

#include <arpa/inet.h>

const char *const port = "3490"; // the port client will be connecting to
const int MAX_DATA_LEN = 100;
const char *arg0;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// loop through all the results and connect to the first we can
// return connected ip address
int get_sock(struct addrinfo *p)
{
	int last_err, fd;
	for (; p != NULL; p = p->ai_next) {
		if (-1 == (fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))) {
			last_err = errno;
			continue;
		}
		if (-1 == connect(fd, p->ai_addr, p->ai_addrlen)) {
			close(fd);
			last_err = errno;
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		fprintf(stderr, "%s\n", strerror(last_err));
		exit(2);
	}

	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	return fd;
}

void get_network(struct addrinfo **out, struct addrinfo *hints, const char *const ip)
{
	int error;
	if ((error = getaddrinfo(ip, port, hints, out)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
		exit(1);
	}
}

void recv_print(int fd)
{
	char buf[MAX_DATA_LEN];
	int bytes;
	while (1){
		bytes = recv(fd, buf, MAX_DATA_LEN, 0);
		if (bytes <= 0)
			break;
		printf("%.*s", bytes, buf);
	}
	if (bytes == -1)
		perror("recv final:");
}

void usage(void)
{
	fprintf(stderr,"usage: %s: hostname\n", arg0);
	exit(1);
}

int main(int argc, char *argv[])
{
	arg0 = argv[0];
	if (argc != 2)
		usage();

	struct addrinfo hints, *network_info;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	get_network(&network_info, &hints, argv[1]);

	int sock = get_sock(network_info);

	freeaddrinfo(network_info);
	network_info = NULL;

	char buf[MAX_DATA_LEN];
	char *hello = "#include <stdio.h>\nint main(void){puts(\"Hello, World!\"); return 2;}\n";

	snprintf(buf, MAX_DATA_LEN, "%s", hello);
	if (send(sock, buf, strlen(buf), 0) == -1)
		perror("client: send()");

	puts("client: receiving");
	recv_print(sock);

	close(sock);
	return 0;
}
