// server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h> // Include for srand() and rand()

#define BUFFER_SIZE 1024
#define PACKET_SIZE 2048

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

int string_to_packet(const char *buffer, size_t buffer_size, struct packet *pkt) {
    // Parse the header part
    int header_size = sscanf(buffer, "%u:%u:%u:%[^:]:", 
                             &pkt->total_frag, &pkt->frag_no, &pkt->size, pkt->filename);

    if (header_size != 4) {
        // Did not parse the correct number of items
        return -1;
    }

    // Calculate the position of filedata
    const char *data_ptr = strchr(buffer, ':');
    for (int i = 0; i < 3; ++i) {
        if (data_ptr != NULL) {
            data_ptr = strchr(data_ptr + 1, ':');
        }
    }

    // If data_ptr is NULL, the packet format is wrong
    if (data_ptr == NULL) {
        return -1;
    }

    // Move pointer to the start of data
    data_ptr++;

    // Calculate the header size based on the data_ptr position
    header_size = data_ptr - buffer;

    // Ensure that the buffer is large enough for the header and the binary data
    if (header_size + pkt->size > buffer_size) {
        // Not enough space
        return -1;
    }

    // Copy the binary data from the buffer into the packet's filedata
    memcpy(pkt->filedata, data_ptr, pkt->size);

    // Return the total size of the parsed packet
    return header_size + pkt->size;
}

double uniform_rand() {
    return rand() / ((double) RAND_MAX + 1); // Uniformly distribute between 0 and 1
}

int main(int argc, char *argv[]) {

    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[BUFFER_SIZE] = {0};
    char packet_buffer[PACKET_SIZE] = {0};
    socklen_t addr_size;
    int nBytes;
    struct packet pkt;
    FILE *file = NULL;

    // Seed the random number generator
    srand(time(NULL));

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

        while(1){
            addr_size = sizeof(clientAddr);
            if(recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *)&clientAddr, &addr_size) < 0){
                perror("Error in receiving data");
                continue;
            }
            
            // Simulate packet drop
            if (uniform_rand() > 1e-2) {
                //printf("Packet received and accepted.\n");


                if(string_to_packet(packet_buffer,PACKET_SIZE,&pkt) == -1){
                    perror("failed to convert string to packet");
                    continue;
                }

                // Send ACK
                if(sendto(sockfd, "ACK", strlen("ACK"), 0, (struct sockaddr *)&clientAddr, addr_size) < 0){
                    perror("Error in sending ACK");
                }
                
                //printf("%s",packet_buffer);

                // Open file on first packet
                if (pkt.frag_no == 1) {
                    file = fopen(pkt.filename, "wb");
                    if (!file) {
                        perror("Failed to open file");
                        continue;
                    }
                }

                // Write data to file
                if (file) {
                    fwrite(pkt.filedata, 1, pkt.size, file);

                    if (pkt.frag_no == pkt.total_frag) {
                        fclose(file);
                        file = NULL;
                        break;
                    }
                }
            } 
            else {
                printf("Simulated packet drop.\n");
                // Do not send ACK to simulate the drop
                continue; // Skip the rest of the processing
            }
        }
    }

    close(sockfd);
    return 0;
}


