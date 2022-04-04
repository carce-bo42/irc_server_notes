#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVPORT 6667

/* from  https://programmerclick.com/article/16241178250/ */

int main(int argc, char **argv)
{
    int i, sockfd;
    struct hostent *host;
    struct sockaddr_in serv_addr;

	char hostname[64];
	if (gethostname(hostname, 64) != 0) {
		printf("gethostname error\n");
		exit(0);
	}

	printf("hostname : %s\n", hostname);

    if ((host = gethostbyname(hostname)) == NULL) {
        printf("gethostbyname error\n");
        exit(0);
    }

    printf("official name: %s\n\n", host->h_name);
    printf("address length: %d bytes\n\n", host->h_length);

    printf("host name alias: \n");
    for (i = 0; host->h_aliases[i]; i++) {
        printf("%s\n", host->h_aliases[i]);
    }
    printf("\naddress list: \n");

    for (i = 0; host->h_addr_list[i]; i++) {
        /*if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("socket error\n");
            exit(0);
        }*/
        // Primero borre, luego use struct sockaddr_in para completar el valor
        /*bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVPORT);*/
        /* h_addr_list [i] apunta al tipo in_addr */
        /*serv_addr.sin_addr = *((struct in_addr *)host->h_addr_list[i]);
        const char *ip = inet_ntoa(serv_addr.sin_addr);
        printf("connect to %s  ", ip);*/
        // Cuando el sistema llame, convierta sockaddr_in en sockaddr
        /*if (connect(sockfd, (struct sockaddr *)&serv_addr,
                    sizeof(struct sockaddr)) == -1) {
            printf("error\n");
            exit(0);
        }
        printf("success\n");*/
        printf("address: [%s]\n", inet_ntoa(*(struct in_addr *)host->h_addr_list[i]));
    }

    return 0;
}