#include <iostream>
#include <thread>

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

#include "helpers.h"
#include "recvutil.h"


using namespace std;

int socket_fd;
struct sockaddr_in server_address, client_address;

void send_ack() {
    // receiving frame component
    char frame[MAX_FRAME_SIZE];
    char data[MAX_DATA_SIZE];
    char ack[ACK_SIZE];
    int frame_size;
    int data_size;
    // etc
    int frame_sequence_num;
    bool frame_error;
    bool eot;
    socklen_t client_address_size;

    // listen data then send ack
    while (true) {
        frame_size = recvfrom(socket_fd, (char *)frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_address, &client_address_size);
        frame_error = read_frame(&frame_sequence_num, data, &data_size, &eot, frame);
        create_ack(frame_sequence_num, ack, frame_error);
        sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_address, client_address_size);
    }
}

void final_checker() {
    thread waiting_thread(send_ack);
    time_stamp start = current_time();

    while (elapsed_time(current_time(), start) < CLARIFYING_TIME) {
        cout << "\r" << "CLARIFYING  " << flush;
        sleep_for(700);
        cout << "\r" << "CLARIFYING ? " << flush;
        sleep_for(700);
        
    }
    waiting_thread.detach();
}

int main(int argc, char * argv[]) {
    int rws;
    int max_buff_size;
    int port;
    char *file_name;

    // reading inputs
    if (argc == 5) {
        file_name = argv[1];
        rws = (int) atoi(argv[2]);
        max_buff_size = MAX_DATA_SIZE * (int) atoi(argv[3]);
        port = atoi(argv[4]);
    } else {
        cout << "Format Input: ./recvfile <filename> <window_size> <buff_size> <port>" << endl;
        return 1;
    }

    // initiating socket
    initiating_socket (server_address, client_address, port, socket_fd);

    // reading all bytes on a file
    FILE *file = fopen(file_name, "wb");
    char buff[max_buff_size];
    int buff_size;

    // initial sliding window
        // receiving format
    char frame[MAX_FRAME_SIZE];
    char data[MAX_DATA_SIZE];
    char ack[ACK_SIZE];
    int frame_size;
    int data_size;
        // pointer
    int lfr, laf;
    int frame_sequence_num;
        //etc
    bool eot;
    bool frame_error;

    // receiving all data 
    // initialization
    bool done_receiving = false;
    int buff_counter = 0;
    while (!done_receiving) {
        buff_size = max_buff_size;
        memset(buff, 0, buff_size);
    
        int frame_sequence_count = (int) max_buff_size / MAX_DATA_SIZE;
        bool win_receive_flag[rws];
        for (int i = 0; i < rws; i++) {
            win_receive_flag[i] = false;
        }
        lfr = -1;
        laf = lfr + rws;
        
        // filling current buff with SLIDING WINDOW PROTOCOL
        while (true) {
            socklen_t client_address_size;
            frame_size = recvfrom(socket_fd, (char *) frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_address, &client_address_size);
            frame_error = read_frame(&frame_sequence_num, data, &data_size, &eot, frame);

            create_ack(frame_sequence_num, ack, frame_error);
            sendto(socket_fd, ack, ACK_SIZE, 0, 
                    (const struct sockaddr *) &client_address, client_address_size);

            if (frame_sequence_num <= laf) {
                if (!frame_error) {
                    int buff_shift = frame_sequence_num * MAX_DATA_SIZE;
                    if (frame_sequence_num == lfr + 1) {
                        memcpy(buff + buff_shift, data, data_size);
                        int shift = 1;
                        for (int i = 1; i < rws; i++) {
                            if (!win_receive_flag[i]) {
                                break;
                            }
                            shift += 1;
                        }
                        for (int i = 0; i < rws; i++){
                            if (i < rws - shift) {
                                win_receive_flag[i] = win_receive_flag[i + shift];
                            }
                            else if (i >= rws - shift) {
                                win_receive_flag[i] = false;
                            }
                        }
                        lfr += shift;
                        laf = lfr + rws;
                    } else if (frame_sequence_num > lfr + 1) {
                        if (!win_receive_flag[frame_sequence_num - (lfr + 1)]) {
                            memcpy(buff + buff_shift, data, data_size);
                            win_receive_flag[frame_sequence_num - (lfr + 1)] = true;
                        }
                    }

                    /* Set max sequence to sequence of frame with EOT */ 
                    if (eot) {
                        buff_size = buff_shift + data_size;
                        frame_sequence_count = frame_sequence_num + 1;
                        done_receiving = true;
                    }
                }
            }
            
            // Move to next buff if all frames in current buff has been received
            if (lfr >= frame_sequence_count - 1) {
                break;
            }
        }


        cout << "\r" << "RECEIVING :" << (unsigned long long) buff_counter * (unsigned long long) 
            max_buff_size + (unsigned long long) buff_size << " BYTES";
        
        fwrite(buff, 1, buff_size, file);
        buff_counter += 1;
    }
    cout << endl;
    fclose(file);
    //in case the last ack is not received by the sender
    final_checker();

    cout << "\ndone" << endl;
    return 0;
}

