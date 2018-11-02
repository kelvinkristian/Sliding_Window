#ifndef SENDUTIL_H
#define SENDUTIL_H

int create_frame(int seq_num, char *frame, char *data, int data_size);
bool read_ack(int *seq_num, bool *neg, char *ack);

#endif