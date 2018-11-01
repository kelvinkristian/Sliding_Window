#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include "helpers.h"
#define TIMEOUT 10
#define SOH 0x1

using namespace std;

//Variabel global
int sock_fd,window_size,lar,lfs;
struct sockaddr_in server_address, client_address;
bool* win_ack_recieved;
time_stamp *win_time_sent;
time_stamp Curr = now_time();
mutex win_mutex;

//Fungsi untuk mendengar ACK
void listen_to_ack(){
    int ack_size, ack_seqnum;
    char ack_frame[MAX_ACK_SIZE];
    bool error, negative_ack;

    while(true){
        socklen_t server_address_size;
        ack_size = recvfrom(sock_fd,(char *)ack_frame,MAX_ACK_SIZE,MSG_WAITALL, (struct sockaddr *) &server_address, &server_address_size);
        error = read_ack(ack_frame,&ack_seqnum,&negative_ack);

        //lock dengan mutex karena ada ack
        win_mutex.lock();
        if(!error && (ack_seqnum>lar) && (ack_seqnum <= lfs)) {
            if(!negative_ack){
                win_ack_recieved[ack_seqnum - (lar + 1)] = true;
            }else{
                win_time_sent[ack_seqnum - (lar + 1)] = Curr;
            }
        }
        win_mutex.unlock();
    }
}

int main(int argc, char const *argv[]){
    int destination_port,max_buffer_size;
    const char *destination_ip;
    const char *filename;
    struct hostent *destination_hostent;
    bool read;

    //Inisialisasi server dan client address dengan 0
    memset(&server_address, 0, sizeof(server_address));
    memset(&client_address, 0, sizeof(client_address));

    //Inisialisasi varibel buffer dan sizenya
    char buffer[max_buffer_size];
    int buffer_size;

    //Inisialisasi frame
    char frame[MAX_FRAME_SIZE];
    int frame_size;

    //Inisialisasi data sebagai container
    char data[MAX_DATA_SIZE];
    int data_size;

    //Cek jumlah argumen
    if (argc != 6){
        perror("usage : ./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>");
        return 1;
    }else{ //Masukkan data argumen ke variabel
        filename = argv[1];
        window_size = atoi(argv[2]);
        max_buffer_size = atoi(argv[3]) * MAX_DATA_SIZE;
        destination_ip = argv[4];
        destination_port = atoi(argv[5]);
    }

    //Pencarian host
    if((destination_hostent = gethostbyname(destination_ip)) == NULL) {
        perror("pencarian host gagal!");
        return 1;
    }

    //Assign atribut server(receiver) dan client(sender) addresses
    
    /*assign atribut server address*/
    server_address.sin_family = AF_INET;
    bcopy(destination_hostent->h_addr,(char *) &server_address.sin_addr, destination_hostent->h_length);
    server_address.sin_port = htons(destination_port);
    /*assign atribut client address*/
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = INADDR_ANY;
    client_address.sin_port = htons(0);

    //buat socket file descriptor(sock_fd)
    if(sock_fd = socket(AF_INET, SOCK_DGRAM, 0) < 0){
        perror("error: pembuatan socket gagal");
        return 1;
    }

    //bind socket ke client
    if(bind(sock_fd, (const struct sockaddr *) &client_address, sizeof(client_address)) < 0){
        perror("error: binding socket gagal");
        return 1;
    }

    //cek ketersediaan file
    if(access(filename,F_OK) == -1){
        perror("error: file tidak ditemukan");
        return 1;
    }

    //buka file untuk dibaca(per byte)
    FILE *myfile = fopen(filename,"rb");
    read = false;
    
    //buat thread untuk mendengar ACK
    thread receive_ack(listen_to_ack);

    //Looping hingga seluruh file selesai dikirim
    while(!read){
        
    }


    return 0;
}

