// deliver.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h> // Include for setsockopt() timeout


#define BUFFER_SIZE 1024
#define DATA_SIZE 1000
#define PACKET_SIZE 2048

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char filename[256];
    char filedata[DATA_SIZE];
};

int packet_to_string(const struct packet *pkt, char *buffer, size_t buffer_size) {
    // The packet format is "total_frag:frag_no:size:filename:filedata"
    int header_size = snprintf(buffer, buffer_size, "%u:%u:%u:%s:", 
                               pkt->total_frag, pkt->frag_no, pkt->size, pkt->filename);

    if (header_size < 0 || header_size >= buffer_size) {
        // Error occurred or buffer is not large enough
        return -1;
    }

    // Ensure that the buffer is large enough for the header and the binary data
    if (header_size + pkt->size > buffer_size) {
        // Not enough space
        return -1;
    }

    // Copy the binary data directly after the header
    memcpy(buffer + header_size, pkt->filedata, pkt->size);

    // Return the total size of the stringified packet
    return header_size + pkt->size;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];
    char packet_buffer[PACKET_SIZE];
    socklen_t addr_size;
    char fileName[256];
    FILE *file;
    int total_fragments = 0;
    char ack[20];

    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server address> <server port number>\n", argv[0]);
        return 1;
    }

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);

    // Set timeout for recvfrom
    struct timeval tv;
    tv.tv_sec = 2; // Example timeout value of 2 seconds
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        close(sockfd);
        return 1;
    }

    // Get input from the user
    printf("Enter ftp <file name>: ");
    fgets(input, BUFFER_SIZE, stdin);

    // Extract the command and file name
    sscanf(input, "%s %s", buffer, fileName);

    // Check if the command is correct and the file exists
    if (strcmp(buffer, "ftp") == 0) {
        file = fopen(fileName, "r");
        if (file) {

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
            
            //Get file size
            fseek(file, 0, SEEK_END);
            long int fileSize = ftell(file);
            rewind(file);

            //calculate # of fragments needed to transfer file.
            total_fragments = (fileSize + DATA_SIZE-1)/ DATA_SIZE;

            struct packet packet;

            //prepare packets for each fragment
            for(int sequenceNumber = 1; sequenceNumber <= total_fragments; sequenceNumber++){
                //reset memory
                memset(&packet, 0, sizeof(packet));

                packet.total_frag = total_fragments;
                packet.frag_no = sequenceNumber;
                packet.size = fread(packet.filedata,1,DATA_SIZE,file);
                strcpy(packet.filename,fileName);
                
                if(packet_to_string(&packet,packet_buffer,PACKET_SIZE) == -1){
                    perror("conversion to string failed");
                    fclose(file);
                    close(sockfd);
                    return 1;
                }

                // //send packet to server
                // if (sendto(sockfd,packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
                //     perror("Sendto failed");
                //     fclose(file);
                //     close(sockfd);
                //     return 1;
                // }

                // //Wait for server's ACK response
                // addr_size = sizeof(serverAddr);
                // if (recvfrom(sockfd, ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size) < 0) {
                //     perror("Recvfrom failed");
                //     fclose(file);
                //     close(sockfd);
                //     return 1;
                // }    
                

                int ack_received = -1;
                do{
                        //send packet to server
                    if (sendto(sockfd,packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
                        perror("Sendto failed");
                        fclose(file);
                        close(sockfd);
                        return 1;
                    }
                    if(ack_received == 0){
                        printf("retransmitted message\n");
                    }

                    // Attempt to receive ACK
                    addr_size = sizeof(serverAddr);
                    if (recvfrom(sockfd, ack, sizeof(ack), 0, (struct sockaddr *)&serverAddr, &addr_size) < 0) {
                        perror("ACK recvfrom failed, possibly due to timeout");
                        printf("Resending packet %d due to timeout.\n", sequenceNumber);
                        ack_received = 0; //indicates that we will retransmit message
                        // ACK not received, will loop to resend
                    } else {        
                        ack[sizeof(ack) - 1] = '\0';

                        if (strcmp(ack, "ACK") == 0) {
                            ack_received = 1; // Correct ACK received, proceed to next packet
                            //printf("ACK received for packet %d\n", sequenceNumber);
                        } else {
                            // In case ACK is not as expected, print an error and prepare to resend
                            printf("Unexpected message received. Resending packet %d.\n", sequenceNumber);
                        }
                        }
                    }while (!ack_received); // Loop until ACK is correctly received
                }

            printf("File transfer completed.\n");
            fclose(file);//put at end
        }
        } 
        else {
            printf("File '%s' not found.\n", fileName);
        }

    close(sockfd);
    return 0;
}
