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
const int max_data_size = 100; // TODO: only used for recv buf display all data from the connection in

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

void better_name(char *buf);

int main(int argc, char *argv[])
{

    if (argc != 2) {
        fprintf(stderr,"usage: %s: hostname\n", argv[0]);
        exit(1);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv;
    struct addrinfo *servinfo;
    if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    struct addrinfo *p;
    int sockfd;  
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
 

    char *string = "\
#include <stdio.h>\n\
int main(void){puts(\"Hello, World!\"); return 2;}\n";
		    char to_send[256*2+1];
    snprintf(to_send,256*2,"%s",string);
    if(send(sockfd, to_send, strlen(to_send), 0) == -1)
	    perror("client: send()");

    puts("client: receiving");

    char buf[max_data_size] = {0};
    ssize_t tmp = recv(sockfd, buf, 20,0);

    while(tmp > 0){
	    printf("%s", buf);

	    write_zero(buf, max_data_size);
	    tmp = recv(sockfd, buf, 20,0);
    }
    if(tmp == -1)
	    perror("recv final:");


    close(sockfd);
    exit(EXIT_SUCCESS);

    // end here

    close(sockfd);

    return 0;
}
