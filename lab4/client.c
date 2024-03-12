#include "packet.h"
#include "client.h"

char buf[BUF_SIZE];
bool insession = false;

int main() {
	char *pch;
	int toklen;
	int socketfd = INVALID_SOCKET;
	pthread_t receive_thread;

	while (true) {
        // Get stdin input (leave 1 byte for \0)
        fgets(buf, BUF_SIZE - 1, stdin);
	    buf[strcspn(buf, "\n")] = 0;    // Find \n and replace it with \0

        pch = buf;
        while (*pch == ' ') pch++;      // edge case: ignore leading spaces
        if (*pch == 0) continue;        // edge case: ignore empty line
        
        // Space delimiter tokens
		pch = strtok(buf, " ");
		toklen = strlen(pch);

		if (strcmp(pch, LOGIN_CMD) == 0) {
			login(pch, &socketfd, &receive_thread);
		} else if (strcmp(pch, LOGOUT_CMD) == 0) {
			logout(&socketfd, &receive_thread);
		} else if (strcmp(pch, JOINSESSION_CMD) == 0) {
			joinsession(pch, &socketfd);
		} else if (strcmp(pch, LEAVESESSION_CMD) == 0) {
			leavesession(socketfd);
		} else if (strcmp(pch, CREATESESSION_CMD) == 0) {
			createsession(socketfd);
		} else if (strcmp(pch, LIST_CMD) == 0) {
			list(socketfd);
		} else if (strcmp(pch, QUIT_CMD) == 0) {
			logout(&socketfd, &receive_thread);
			break;
		} else {
			// restore the buffer to send the original text
			buf[toklen] = ' ';
            send_text(socketfd);
		}
	}
	fprintf(stdout, "Terminating connection.\n");
	return 0;
}

/****************************************************************************
    * Description: Thread function that listens to the server
    *              and prints out the received packets.
    (receive may get: JN_ACK, JN_NAC, NS_ACK, QU_ACK, MESSAGE)
    * Parameters: socketfd_void_p - a pointer to the socket file descriptor
*****************************************************************************/
void *receive(void *socketfd_void_p) {
    int *socketfd_p = (int *)socketfd_void_p;
    int numbytes;
    Packet packet;
    while (true) {	
        if ((numbytes = recv(*socketfd_p, buf, BUF_SIZE - 1, 0)) == -1) {
            fprintf(stderr, "client receive: recv\n");
            return NULL;
        }
        if (numbytes == 0) continue;
        buf[numbytes] = 0;
        stringToPacket(buf, &packet);
        
        if (packet.type == JN_ACK) {
            fprintf(stdout, "Successfully joined session %s.\n", packet.data);
            insession = true;
        } else if (packet.type == JN_NAK) {
            fprintf(stdout, "Join failure. Detail: %s\n", packet.data);
            insession = false;
        } else if (packet.type == NS_ACK) {
            fprintf(stdout, "Successfully created and joined session %s.\n", packet.data);
            insession = true;
        } else if (packet.type == QU_ACK) {
            fprintf(stdout, "User id\t\tSession ids\n%s", packet.data);
        } else if (packet.type == MESSAGE){   
            fprintf(stdout, "%s: %s\n", packet.source, packet.data);
        } else {
            fprintf(stdout, "Unexpected packet received: type %d, data %s\n",
            packet.type, packet.data);
        }
        // solve the ghost-newliner 
        fflush(stdout);
	}
	return NULL;
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
	}
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/****************************************************************************
    * Description: Log in to the server
    * Parameters: pch - tokenized command
    *             socketfd_p - a pointer to the socket file descriptor
    *             receive_thread_p - a pointer to the receive thread
*****************************************************************************/
void login(char *pch, int *socketfd_p, pthread_t *receive_thread_p) {
	char *client_id, *password, *server_ip, *server_port;
	pch = strtok(NULL, " ");
	client_id = pch;
	pch = strtok(NULL, " ");
	password = pch;
	pch = strtok(NULL, " ");
	server_ip = pch;
	pch = strtok(NULL, " \n");
	server_port = pch;

	if (client_id == NULL || password == NULL || server_ip == NULL || server_port == NULL) {
		fprintf(stdout, "Usage: /login <client_id> <password> <server_ip> <server_port>\n");
		return;
	} else if (*socketfd_p != INVALID_SOCKET) {
		fprintf(stdout, "Already logged in to a server");
		return;
	} else {
		// Connect TCP
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Socket creation failed");
            return;
        }

        // Configure server address structure
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(atoi(server_port));
        serverAddr.sin_addr.s_addr = inet_addr(server_ip);

        socklen_t serverAddrLength = sizeof(serverAddr);

        // Attempt to connect to the server with TCP
        if (connect(sockfd, (struct sockaddr *) &serverAddr, serverAddrLength) < 0) {
            perror("Create TCP connection failed");
            close(sockfd);
            return;
        }

        ///////////////////////////////////////////////////////////////////////////

        // Send LOGIN packet
		int numbytes;
        Packet packet;
        packet.type = LOGIN;
        strncpy(packet.source, client_id, MAX_NAME);
        strncpy(packet.data, password, MAX_DATA);
        packet.size = strlen(packet.data);
        packetToString(&packet, buf);
		if ((numbytes = send(*socketfd_p, buf, BUF_SIZE - 1, 0)) == -1) {
			fprintf(stderr, "client: send\n");
			close(*socketfd_p);
            *socketfd_p = INVALID_SOCKET;
			return;
		}

		if ((numbytes = recv(*socketfd_p, buf, BUF_SIZE - 1, 0)) == -1) {
			fprintf(stderr, "client: recv\n");
			close(*socketfd_p);
            *socketfd_p = INVALID_SOCKET;
			return;
		}
        buf[numbytes] = 0; 
        stringToPacket(buf, &packet);

        if (packet.type == LO_ACK && pthread_create(receive_thread_p, NULL, receive, socketfd_p) == 0) {
            fprintf(stdout, "login successful.\n");  
        } else if (packet.type == LO_NAK) {
            fprintf(stdout, "login failed. Detail: %s\n", packet.data);
            close(*socketfd_p);
            *socketfd_p = INVALID_SOCKET;
            return;
        } else {
            fprintf(stdout, "Unexpected packet received: type %d, data %s\n", packet.type, packet.data);
            close(*socketfd_p);
            *socketfd_p = INVALID_SOCKET;
            return;
        }
	}
}

void logout(int *socketfd_p, pthread_t *receive_thread_p) {
	if (*socketfd_p == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} 

	int numbytes;
    Packet packet;
    packet.type = EXIT;
    packet.size = 0;
    packetToString(&packet, buf);
	
	if ((numbytes = send(*socketfd_p, buf, BUF_SIZE - 1, 0)) == -1) {
		fprintf(stderr, "client: send\n");
		return;
	}	
	if (pthread_cancel(*receive_thread_p)) {
        fprintf(stderr, "logout leavesession failure\n");
	} else {
        fprintf(stdout, "logout leavesession success\n");	
	} 
    insession = false;
	close(*socketfd_p);
	*socketfd_p = INVALID_SOCKET;
}

void joinsession(char *pch, int *socketfd_p) {
	if (*socketfd_p == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} else if (insession) {
        fprintf(stdout, "You have already joined a session.\n");
        return;
    }

    // Extract session name
	char *session_id;
	pch = strtok(NULL, " ");
	session_id = pch;
	if (session_id == NULL) {
		fprintf(stdout, "usage: /joinsession <session_id>\n");
	} else {
        // Join session packet (JOIN)
		int numbytes;
        Packet packet;
        packet.type = JOIN;
        strncpy(packet.data, session_id, MAX_DATA);
        packet.size = strlen(packet.data);
        packetToString(&packet, buf);
		if ((numbytes = send(*socketfd_p, buf, BUF_SIZE - 1, 0)) == -1) {
			fprintf(stderr, "client: send\n");
			return;
		}
	}
}

void leavesession(int socketfd) {
	if (socketfd == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} else if (!insession) {
        fprintf(stdout, "You have not joined a session yet.\n");
        return;
    }

	int numbytes;
	Packet packet;
    packet.type = LEAVE_SESS;
    packet.size = 0;
    packetToString(&packet, buf);
        if ((numbytes = send(socketfd, buf, BUF_SIZE - 1, 0)) == -1) {
            fprintf(stderr, "client: send\n");
            return; 
        }
    insession = false;
}

void createsession(int socketfd) {
	if (socketfd == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} else if (insession) {
        fprintf(stdout, "You have already joined a session.\n");
        return;
    }
	
    int numbytes;
    Packet packet;
    packet.type = NEW_SESS;
    packet.size = 0;
    packetToString(&packet, buf);		
    if ((numbytes = send(socketfd, buf, BUF_SIZE - 1, 0)) == -1) {
        fprintf(stderr, "client: send\n");
        return; 
    } 
}

void list(int socketfd) {
	if (socketfd == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} 
	int numbytes;
    Packet packet;
    packet.type = QUERY;
    packet.size = 0;
    packetToString(&packet, buf);
	
	if ((numbytes = send(socketfd, buf, BUF_SIZE - 1, 0)) == -1) {
		fprintf(stderr, "client: send\n");
		return; 
	}
}

void send_text(int socketfd) {
	if (socketfd == INVALID_SOCKET) {
		fprintf(stdout, "You have not logged in to any server.\n");
		return;
	} else if (!insession) {
        fprintf(stdout, "You have not joined a session yet.\n");
        return;
    }

	int numbytes;
    Packet packet;
    packet.type = MESSAGE;
    
    strncpy(packet.data, buf, MAX_DATA);
    packet.size = strlen(packet.data); 
    packetToString(&packet, buf);	
	if ((numbytes = send(socketfd, buf, BUF_SIZE - 1, 0)) == -1) {
		fprintf(stderr, "client: send\n");
		return; 
	}
}