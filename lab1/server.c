// server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {

    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_size;
    int nBytes;

    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <UDP listen port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  //AF_NET -- IPV4 internet protocol, SOCK_DGRAM -- type of socket: UDP 
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;                //-- IPV4 internet protocol
    serverAddr.sin_port = htons(atoi(argv[1]));     // port number to listen to
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces

    socklen_t serverAddrLength = sizeof(serverAddr);

    // Bind socket with address struct (address = IP + port Number)
    if (bind(sockfd, (const struct sockaddr *) &serverAddr, serverAddrLength) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Initialize size variable to be used later on
    addr_size = sizeof(clientAddr);

    printf("Server is listening on port %s\n", argv[1]); //output message to show that server is active and ready to listen

    while (1) {
        // Try to receive any incoming UDP datagram. Address and port of requesting client will be stored on clientAddr
        nBytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &clientAddr, &addr_size);
        
        if (nBytes < 0) {
            perror("Error in receiving data");
            continue;
        }

        buffer[nBytes] = '\0'; // Null-terminate the received data

        // Respond to the client based on the message received
        if (strcmp(buffer, "ftp") == 0) {
            strcpy(buffer, "yes");
        } else {
            strcpy(buffer, "no");
        }

        // Send the response back to the client
        nBytes = sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &clientAddr, addr_size);

        if (nBytes < 0) {
            perror("Error in sending data");
        }
    }

    close(sockfd);
    return 0;
}


