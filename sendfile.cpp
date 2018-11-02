#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include "helpers.h"

//MACRO
#define UNDEFINED -1
#define TIMEOUT 10
#define SOH 0x1

using namespace std;

//Variabel global
struct sockaddr_in server_address, client_address;
int sock_fd,window_size,lar,lfs;
time_stamp Curr = now_time();
time_stamp *win_time_sent;
bool *win_ack_recieved;
bool *win_is_sent;
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
    int destination_port,max_buffer_size,shift;
    struct hostent *destination_hostent;
    const char *destination_ip;
    const char *filename;
    bool read,sent;

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
        filename            = argv[1];
        window_size         = atoi(argv[2]);
        max_buffer_size     = atoi(argv[3]) * MAX_DATA_SIZE;
        destination_ip      = argv[4];
        destination_port    = atoi(argv[5]);
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
        
        //Inisialisasi frame sliding window
        int seqnum,seqcount;

        win_mutex.lock();
        //inisialisasi seqcount
        seqcount = (buffer_size/MAX_DATA_SIZE) + (buffer_size%MAX_DATA_SIZE != 0) ? 1 : 0; 

        //inisialisasi mask pada window
        win_time_sent = new time_stamp[window_size];
        win_ack_recieved = new bool[window_size];
        win_is_sent = new bool[window_size];

        //assign semua arr bool menjadi false
        memset(win_ack_recieved,false,sizeof(win_ack_recieved));        
        memset(win_is_sent,false,sizeof(win_is_sent));

        //inisialisasi LAR dan LFS untuk sender
        lar = UNDEFINED;
        lfs = lar + window_size;
        
        win_mutex.unlock();


        //Baca file ke buffer yang tersedia(per 1 byte * max_buffer_size)
        buffer_size = fread(buffer,1,max_buffer_size,myfile);
        if(buffer_size < max_buffer_size){
            read = true;
        }else if(buffer_size == max_buffer_size){
            //variabel temporer untuk checking
            char x[1];
            int temp;

            //cek file apakah masih ada setelah membaca sebanyak max_buffer_size
            if((temp = fread(x,1,1,myfile)) == 0){
                read = true;
            }
            //mundurkan pointer pada file sebesar offset:-1
            temp = fseek(myfile,-1,SEEK_CUR);
        }

        //Send data ke server

        //inisialisasi sent = false
        sent = false;

        while(!sent){
            
            win_mutex.lock();
            //Shift variabel jika terdapat ACK
            if(win_ack_recieved[0]){
                
                //init shift = 1, karena ack pertama sudah didapatkan
                shift = 1;
                int i = 1;
                
                //cek berapa banyak yang sudah ACK dari elemen kedua
                while((i < window_size) && win_ack_recieved[i]){
                    shift++;
                    i++;
                }
                //assign mask dan stamp dari window sesuai dengan pergeseran
                for(i = 0; i < window_size-shift; i++){
                    win_ack_recieved[i] = win_ack_recieved[i+shift];
                    win_time_sent[i] = win_time_sent[i+shift];
                    win_is_sent[i] = win_is_sent[i+shift];
                }
                //assign mask frame yang akan masuk ke window dengan false
                memset(win_ack_recieved+(window_size-shift),false,shift);
                memset(win_is_sent+(window_size-shift),false,shift);

                //geser LAR sebesar shift dan update LFS
                lar += shift;
                lfs = lar + window_size;

            }
            win_mutex.unlock();

            //TODO sendframe

        }


    }


    return 0;
}

