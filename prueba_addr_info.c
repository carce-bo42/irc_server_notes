#include <netdb.h> /* struct addrinfo, gethostbyname */
#include <unistd.h> /* gethostname */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_ERROR 0

int main() {

    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_INET;  // AF_UNSPEC = don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;  // fill in my IP for me

    if ((status = getaddrinfo("PC-CARCE", "3490", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)servinfo->ai_addr;
    printf("addr_in : [%s]\n", inet_ntoa(addr_in->sin_addr));

    // servinfo now points to a linked list of 1 or more struct addrinfos

    // ... do everything until you don't need servinfo anymore ....

    freeaddrinfo(servinfo); // free the linked-list
}