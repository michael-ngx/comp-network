// deliver.c

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
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];
    socklen_t addr_size;
    char fileName[256];
    FILE *file;

    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server address> <server port number>\n", argv[0]);
        return 1;
    }

    // Create UDP socket
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);

    // Get input from the user
    printf("Enter the command: ");
    fgets(input, BUFFER_SIZE, stdin);

    // Extract the command and file name
    sscanf(input, "%s %s", buffer, fileName);

    // Check if the command is correct and the file exists
    if (strcmp(buffer, "ftp") == 0) {
        file = fopen(fileName, "r");
        if (file) {
            fclose(file);

            // Send message "ftp" to the server
            if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
                perror("Sendto failed");
                close(sockfd);
                return 1;
            }

            // Wait for server's response
            addr_size = sizeof(serverAddr);
            if (recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, &addr_size) < 0) {
                perror("Recvfrom failed");
                close(sockfd);
                return 1;
            }

            buffer[BUFFER_SIZE - 1] = '\0'; // Ensure null-termination

            // Check server's response
            if (strcmp(buffer, "yes") == 0) {
                printf("A file transfer can start.\n");
            } 
            else {
                //printf("Server has denied file transfer.\n");
                close(sockfd);
                return 0;
            }
        } 
        else {
            printf("File '%s' not found.\n", fileName);
        }
    }

    close(sockfd);
    return 0;
}
