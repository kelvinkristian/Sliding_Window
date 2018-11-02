#include <iostream>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "definitions.h"


using namespace std;

int sock_fd;
struct sockaddr_in server_address, client_address;

int window_size;
bool *win_ack_received;
time_stamp *win_time_sent;
int lar, lfs;

time_stamp CURR = current_time();
mutex win_mutex;
bool read_ack(int *seqnum, bool *neg, char *ack);
void listen_to_ack();
int create_frame(int seqnum, char *frame, char *data, int data_size, bool eot);


int main(int argc, char *argv[]) {
    struct hostent *destination_hostent;
    char *destination_ip;
    char *filename;
    int destination_port;
    int max_buffer_size;
    bool read,sent;

    memset(&server_address, 0, sizeof(server_address)); 
    memset(&client_address, 0, sizeof(client_address)); 
    

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
    destination_hostent = gethostbyname(destination_ip);
    if(!destination_hostent) {
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
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        perror("error: pembuatan socket gagal");
        return 1;
    }

    //bind socket ke client
    if(::bind(sock_fd, (const struct sockaddr *) &client_address, sizeof(client_address)) < 0){
        perror("error: binding socket gagal");
        return 1;
    }

    //cek ketersediaan file
    if(access(filename,F_OK) == -1){
        perror("error: file tidak ditemukan");
        return 1;
    }

    /* Open file to send */
    FILE *file = fopen(filename, "rb");

    //Inisialisasi varibel buffer dan sizenya
    char buffer[max_buffer_size];
    int buffer_size;

    //Inisialisasi frame
    char frame[MAX_FRAME_SIZE];
    int frame_size;

    //Inisialisasi data sebagai container
    char data[MAX_DATA_SIZE];
    int data_size;

    /* Start thread to listen for ack */
    thread receive_ack(listen_to_ack);

    /* Send file */
    read = false;
    int buffer_num = 0;
    while (!read) {

        //Baca file ke buffer yang tersedia(per 1 byte * max_buffer_size)
        buffer_size = fread(buffer,1,max_buffer_size,file);
        if(buffer_size < max_buffer_size){
            read = true;
        }else if(buffer_size == max_buffer_size){
            //variabel temporer untuk checking
            char x[1];
            int temp;

            //cek file apakah masih ada setelah membaca sebanyak max_buffer_size
            if((temp = fread(x,1,1,file)) == 0){
                read = true;
            }
            //mundurkan pointer pada file sebesar offset:-1
            temp = fseek(file,-1,SEEK_CUR);
        }
        
        int seqnum,seqcount;

        win_mutex.lock();

        //inisialisasi seqcount
        seqcount = (buffer_size/MAX_DATA_SIZE) + ((buffer_size % MAX_DATA_SIZE == 0) ? 0 : 1);
        
        //inisialisasi mask pada window
        win_time_sent = new time_stamp[window_size];
        win_ack_received = new bool[window_size];
        bool win_is_sent[window_size];

        //assign semua arr bool menjadi false
        memset(win_ack_received,false,sizeof(win_ack_received));        
        memset(win_is_sent,false,sizeof(win_is_sent));

        lar = UNDEFINED;
        lfs = lar + window_size;

        win_mutex.unlock();

        //Send data ke server

        //inisialisasi sent = false
        sent = false;
        
        while (!sent) {

            win_mutex.lock();
            //Shift variabel jika terdapat ACK
            if(win_ack_received[0]){
                
                //init shift = 1, karena ack pertama sudah didapatkan
                int shift = 1;
                int i = 1;
                
                //cek berapa banyak yang sudah ACK dari elemen kedua hingga LFS
                while((i < window_size) && win_ack_received[i]){
                    shift++;
                    i++;
                }
                //assign mask dan stamp dari window sesuai dengan pergeseran
                for(i = 0; i < window_size-shift; i++){
                    win_ack_received[i] = win_ack_received[i+shift];
                    win_time_sent[i] = win_time_sent[i+shift];
                    win_is_sent[i] = win_is_sent[i+shift];
                }
                //assign mask frame yang akan masuk ke window dengan false
                memset(win_ack_received+(window_size-shift),false,shift);
                memset(win_is_sent+(window_size-shift),false,shift);

                //geser LAR sebesar shift dan update LFS
                lar += shift;
                lfs = lar + window_size;

            }
            win_mutex.unlock();

            //Kirim frame 
            int shift_size;
            for(int i = 0; i < window_size; i++){
                
                //inisialisasi seqnum
                seqnum = (lar + 1) + i;

                if(seqnum < seqcount){

                    win_mutex.lock();
                    if(!win_is_sent[i] || (elapsed_time(current_time(), win_time_sent[i])) > TIMEOUT){
                        
                        shift_size = seqnum * MAX_DATA_SIZE;
                        data_size = (buffer_size - shift_size < MAX_DATA_SIZE) ? (buffer_size - shift_size) : MAX_DATA_SIZE;
                        memcpy(data,(buffer + buffer_size),data_size);
                        bool eot = (seqnum == seqcount - 1) && (read);
                        frame_size = create_frame(seqnum, frame, data, data_size, eot);
                        sendto(sock_fd,frame,frame_size,0,(const struct sockaddr *)&server_address,sizeof(server_address));
                        win_is_sent[i] = true;
                        win_time_sent[i] = current_time();

                    }
                    win_mutex.unlock();

                }


            }

            bool eot = (seqnum == seqcount - 1) && (read);
            frame_size = create_frame(seqnum, frame, data, data_size, eot);

            /* Move to next buffer if all frames in current buffer has been acked */
            if (lar >= seqcount - 1){
                sent = true;
            }
        }

        cout << "\r" << "SENDING: " << (unsigned long long) buffer_num * (unsigned long long) max_buffer_size + (unsigned long long) buffer_size << " Bytes" << flush;
        buffer_num += 1;
        if (read) break;
    }
    
    fclose(file);
    delete [] win_ack_received;
    delete [] win_time_sent;
    receive_ack.detach();
    cout << endl << "The file \"" << filename << "\" has been successfully sent!" << endl;
    return 0;
}

bool read_ack(int *seqnum, bool *neg, char *ack) {
    *neg = ack[0] == 0x0 ? true : false;

    uint32_t net_seqnum;
    memcpy(&net_seqnum, ack + 1, 4);
    *seqnum = ntohl(net_seqnum);

    return ack[5] != checksum(ack, ACK_SIZE - (int) 1);
}

void listen_to_ack() {
    char ack[ACK_SIZE];
    int ack_size;
    int ack_seqnum;
    bool ack_error;
    bool ack_neg;

    /* Listen for ack from reciever */
    while (true) {
        socklen_t server_address_size;
        ack_size = recvfrom(sock_fd, (char *)ack, ACK_SIZE, 
                MSG_WAITALL, (struct sockaddr *) &server_address, 
                &server_address_size);
        ack_error = read_ack(&ack_seqnum, &ack_neg, ack);

        win_mutex.lock();

        if (!ack_error && ack_seqnum > lar && ack_seqnum <= lfs) {
            if (!ack_neg) {
                win_ack_received[ack_seqnum - (lar + 1)] = true;
            } else {
                win_time_sent[ack_seqnum - (lar + 1)] = CURR;
            }
        }

        win_mutex.unlock();
    }
}

int create_frame(int seqnum, char *frame, char *data, int data_size, bool eot) {
    if (eot) {
        frame[0] = 0x0;
    } else {
        frame[0] = 0x1;
    }
    uint32_t net_seqnum = htonl(seqnum);
    uint32_t net_data_size = htonl(data_size);
    memcpy(frame + 1, &net_seqnum, 4);
    memcpy(frame + 5, &net_data_size, 4);
    memcpy(frame + 9, data, data_size);
    frame[data_size + 9] = checksum(frame, data_size + (int) 9);

    return data_size + (int)10;
}
