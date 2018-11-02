#include <iostream>
#include <thread>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

#include "helpers.h"
#include "recvutil.h"

using namespace std;

void initiating_socket (struct sockaddr_in &server_address, 
    struct sockaddr_in &client_address, int &port, int &socket_fd) {
    // address initiation
    memset(&server_address, 0, sizeof(server_address)); 
    memset(&client_address, 0, sizeof(client_address)); 
      
    // server_addressess initiation
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; 
    server_address.sin_port = htons(port);

    // socket creation
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "socket creation failed" << endl;
    }

    // bind socket
    if (::bind(socket_fd, (const struct sockaddr *)&server_address, 
            sizeof(server_address)) < 0) { 
        cerr << "socket binding failed" << endl;
    }
}

void create_ack(int seq_num, char *ack, bool error) {
    if (error) {
        ack[0] = 0x0;
    } else {
        ack[0] = 0x1;
    }
    uint32_t net_seq_num = htonl(seq_num);
    memcpy(ack + 1, &net_seq_num, 4);
    ack[5] = checksum(ack, ACK_SIZE - (int) 1);
}

bool read_frame(int *seq_num, char *data, int *data_size, bool *eot, char *frame) {
    if (frame[0] == 0x0) {
        *eot = true;
    } else {
        *eot = false;
    }

    uint32_t net_seq_num;
    memcpy(&net_seq_num, frame + 1, 4);
    *seq_num = ntohl(net_seq_num);

    uint32_t net_data_size;
    memcpy(&net_data_size, frame + 5, 4);
    *data_size = ntohl(net_data_size);

    memcpy(data, frame + 9, *data_size);

    return frame[*data_size + 9] != checksum(frame, *data_size + (int) 9);
}