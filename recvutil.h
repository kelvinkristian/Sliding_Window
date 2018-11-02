#ifndef RECVUTIL_H
#define RECVUTIL_H

using namespace std;

void initiating_socket (struct sockaddr_in &server_address, struct sockaddr_in &client_address, int &port, int &socket_fd);
void create_ack(int seq_num, char *ack, bool error);
bool read_frame(int *seq_num, char *data, int *data_size, bool *eot, char *frame);

#endif