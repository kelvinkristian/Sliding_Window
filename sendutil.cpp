#include "sendutil.h"
#include "definitions.h"

int create_frame(int seq_num, char *frame, char *data, int data_size){
    frame[0] = 0x1;
    uint32_t net_seq_num = htonl(seq_num);
    uint32_t net_data_size = htonl(data_size);
    memcpy(frame + 1, &net_seq_num, 4);
    memcpy(frame + 5, &net_data_size, 4);
    memcpy(frame + 9, data, data_size);
    frame[data_size + 9] = checksum(frame, data_size + (int) 9);

    return data_size + (int)10;
}

bool read_ack(int *seq_num, bool *neg, char *ack){
    *neg = ack[0] == 0x0 ? true : false;

    uint32_t net_seq_num;
    memcpy(&net_seq_num, ack + 1, 4);
    *seq_num = ntohl(net_seq_num);

    return ack[5] != checksum(ack, ACK_SIZE - (int) 1);
}