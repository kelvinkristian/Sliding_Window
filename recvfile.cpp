#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "helpers.h"

#define WAITING_TIME 3000

using namespace std;

int socket_fd;
struct sockaddr_in server_address, client_address;

int main(int argc, char * argv[]) {
    int port;
    int window_size;
    int max_buffer_size;
    char *file_name;

    // reading command
    if (argc == 5) {
        file_name = argv[1];
        window_size = (int) atoi(argv[2]);
        max_buffer_size = MAX_DATA_SIZE;
        port = atoi(argv[4]);
    } else {
        cerr << "usage: ./recvfile <filename> <window_size> <buffer_size> <port>" << endl;
        return 1;
    }

    // inisialisasi server dan client address
    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    // socket properties
    server_address.sin_family =  AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    // create socket
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "socket creation failed" << endl;
        return 1;
    }

    // bind socket to server address
    if(::bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        cerr << "socket binding failed" << endl;
        return 1;
    }

    // define buffer size
    FILE *file = fopen(file_name, "wb");
    chat buffer[max_buffer_size];
    int buffer_size;

    // sliding window preparation
        // receiver component
    char receive_frame[MAX_FRAME_SIZE];
    char receive_data[MAX_DATA_SIZE];
    int receive_frame_size;
    int receive_data_size;
    bool frame_error;
        // ack component
    char ack[ACK_SIZE];
        // receiver pointer
    int lfr, laf;
    int recv_seq_num;
        // extension
    bool eot;

    // receive frames 
    bool receiving_done = false;
    while (!receiving_done) {
        buffer_size = max_buffer_size;
        memset(buffer, 0, buffer_size);

        int sequence_count = (int) max_buffer_size / MAX_DATA_SIZE;
        
        bool already_receive_window[window_size];
        for (int i = 0; i < window_size; i++) {
            already_receive_window[i] = false;
        }
        
        lfr = -1;
        laf = lfr + window_size;

        // receive current buffer
        while (true) {
            socklen_t client_address_size;
            frame_size = recvfrom(socket_fd, (char *) frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_address, &client_address_size);
            frame_error = read_frame(&sequence_count, receive_data, &receive_data_size, &eot, receive_frame)


        }
        
    }
    

    return 0;
}