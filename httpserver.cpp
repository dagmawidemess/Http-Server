#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <semaphore.h>

#define DEFAULT_PORT 80
#define IP_PROTO 0
#define FAILED 0
#define ONE 1
#define TWO 2
#define THRE 3
#define SAME 0
#define NONE 0
#define TUNE 5
#define TRU 1
#define BUFF_SIZE 15000
#define VALID_LEN 27
#define INVALID -1
#define EMPTY '\0'
#define PERMISSION 0666
#define ERR 0

sem_t avail_threads;
sem_t assigned_threads;
pthread_mutex_t lock;
pthread_mutex_t writer_lock;
pthread_mutex_t fileptr_lock;
int num_readers = 0;
char *log_file = NULL;
int32_t log_fptr = 0;


void * handle_request(void* arg){
    while(1){
        sem_wait(&assigned_threads); //assigned_thread.down()

        int64_t clients_socket = *((int *)arg);  //parse fcn params

     
        char temp_buffer[ONE] = {NONE};
        char buffer[BUFF_SIZE] = {NONE};
        int ptr = 0;

        // read request from client
        while (read(clients_socket, temp_buffer, ONE) > 0){
            buffer[ptr] = temp_buffer[0];
            if(buffer[ptr] == '\n' && buffer[ptr - 1] == '\r' && 
                buffer[ptr - 2] == '\n' && buffer[ptr - 3] == '\r'){
                break;
            }
            ptr++;
        }
        write(1, temp_buffer, 1); //delete later
        printf("buffer:\n%s\n", buffer);

        const char s[TWO] = " ";
        char * token;
        char * request;

        // get the first token
        request = strtok(buffer, s);
        char STR_GET[] = "GET";
        char STR_PUT[] = "PUT";

        token = strtok(NULL, s); // get file name
        int8_t valid_rec = NONE;

        int32_t NotValidReq = strcmp(request, STR_GET) != NONE && strcmp(request, STR_PUT) != NONE;

        //handles case with / in the front
        if(strlen(token) == 28 && token[0] == '/'){
            token++;
        }

        //request is not PUT or GET
        if(NotValidReq){
            char temp_buf[3000];

            sprintf(temp_buf, "FAIL: %s %s HTTP/1.1 --- response 500\\n\n", request, token);
            sprintf(temp_buf, "%s========\n", temp_buf);

            pthread_mutex_lock(&fileptr_lock); //lock for shared var
            int32_t cur_fp = log_fptr;    //grab current f ptr
            log_fptr += strlen(temp_buf);    //increment f ptr
            pthread_mutex_unlock(&fileptr_lock);

            int32_t fd1;
            fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);

            pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

            dprintf(clients_socket, "HTTP/1.1 500 Internal Server Error\r\n");
            dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);


        }else{

            // assert file name convention
            if (strlen(token) != VALID_LEN) {

                char temp_buf[3000];

                sprintf(temp_buf, "FAIL: %s %s HTTP/1.1 --- response 400\\n\n",request, token);
                sprintf(temp_buf, "%s========\n", temp_buf);

                //udate file pointer
                pthread_mutex_lock(&fileptr_lock); //lock for shared var
                int32_t cur_fp = log_fptr;    //grab current f ptr
                log_fptr += strlen(temp_buf);    //increment f ptr
                pthread_mutex_unlock(&fileptr_lock);

                //write to file
                int32_t fd1;
                fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);
                pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

                dprintf(clients_socket, "HTTP/1.1 400 Bad Request\r\n");
                dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                valid_rec = TRU;
            } else {
                for (size_t i = 0; i < strlen(token); i++) {
                    if (isalpha(token[i]) == SAME && isdigit(token[i]) == SAME &&
                        token[i] != '-' && token[i] != '_') {

                        char temp_buf[3000];

                        sprintf(temp_buf, "FAIL: %s %s HTTP/1.1 --- response 400\\n\n",request, token);
                        sprintf(temp_buf, "%s========\n", temp_buf);

                        //udate file pointer
                        pthread_mutex_lock(&fileptr_lock); //lock for shared var
                        int32_t cur_fp = log_fptr;    //grab current f ptr
                        log_fptr += strlen(temp_buf);    //increment f ptr
                        pthread_mutex_unlock(&fileptr_lock);

                        //write to file
                        int32_t fd1;
                        fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);
                        pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

                        dprintf(clients_socket, "HTTP/1.1 400 Bad Request\r\n");
                        dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                        valid_rec = TRU;
                    }
                }
            }

            if (valid_rec == NONE) {
                // this is a GET request
                if (strcmp(request, STR_GET) == NONE) {

                    //Reader process
            //        pthread_mutex_lock(&lock); //lock for shared var
            //        num_readers += 1;
            //        if (num_readers == 1){ //if its 1st reader lock writer
            //            pthread_mutex_lock(&writer_lock);
            //        }
            //        pthread_mutex_unlock(&lock);


                    int32_t fd = open(token, O_RDONLY); // open file
                    printf("1st fd: %d", fd);
                    if (fd == INVALID) { // calling GET on non-existent file

                        //files does not exist
                        if(access(token, F_OK) != 0){
                            char temp_buf[3000];

                            sprintf(temp_buf, "FAIL: GET %s HTTP/1.1 --- response 404\\n\n", token);
                            sprintf(temp_buf, "%s========\n", temp_buf);

                            pthread_mutex_lock(&fileptr_lock); //lock for shared var
                            int32_t cur_fp = log_fptr;    //grab current f ptr
                            log_fptr += strlen(temp_buf);    //increment f ptr
                            pthread_mutex_unlock(&fileptr_lock);

                            int32_t fd1;
                            fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);

                            pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

                            dprintf(clients_socket, "HTTP/1.1 404 Not Found\r\n");
                            dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                            close(clients_socket);
                            close(fd);
                        }else if(access(token, R_OK) != 0){ //no read access
                            char temp_buf[3000];

                            sprintf(temp_buf, "FAIL: GET %s HTTP/1.1 --- response 403\\n\n", token);
                            sprintf(temp_buf, "%s========\n", temp_buf);

                            pthread_mutex_lock(&fileptr_lock); //lock for shared var
                            int32_t cur_fp = log_fptr;    //grab current f ptr
                            log_fptr += strlen(temp_buf);    //increment f ptr
                            pthread_mutex_unlock(&fileptr_lock);

                            int32_t fd1;
                            fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);

                            pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

                            dprintf(clients_socket, "HTTP/1.1 403 Forbidden\r\n");
                            dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                            close(clients_socket);
                            close(fd);
                        }

                    } else {
                        
                        struct stat f_sz;
                        fstat(fd, & f_sz);
                        int32_t file_sz = f_sz.st_size;

                        dprintf(clients_socket, "HTTP/1.1 200 OK\r\n");
                        dprintf(clients_socket, "Content-Length: %d\r\n\r\n", file_sz);

                        char buf[ONE] = "";
                        ssize_t sz_read = sizeof(char);

                        sz_read = ONE;

                        while (sz_read > NONE) { // keep reading until EOF file
                            sz_read = read(fd, & buf, sizeof(buf));
                            write(clients_socket, & buf, sz_read);

                            buf[NONE] = EMPTY; // clear buffer
                        }
                        close(fd);

                        if(log_file != NULL){
                            char temp_buf[3000];

                            sprintf(temp_buf, "GET %s length 0\n", token);
                            sprintf(temp_buf, "%s========\n", temp_buf);
                            
                            //lock when editing file pointer
                            pthread_mutex_lock(&fileptr_lock); //lock for shared var
                            int32_t cur_fp = log_fptr;    //grab current f ptr
                            log_fptr += strlen(temp_buf);    //increment f ptr
                            pthread_mutex_unlock(&fileptr_lock);

                            //write to log file
                            fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);
                            pwrite(fd, temp_buf, strlen(temp_buf), cur_fp);
                        }


            //                pthread_mutex_lock(&lock); //lock for shared var
            //                num_readers -= 1;
            //                if (num_readers == 0){ //if its 1st reader lock writer
            //                    pthread_mutex_unlock(&writer_lock);
            //                }
            //                pthread_mutex_unlock(&lock);

                    }

                } else if (strcmp(request, STR_PUT) == SAME) { // this is a put request

                    char * rec_name = token;
                    int32_t check = open(token, O_RDONLY);
                    close(check);

                    //if file exists and has no write permission
                    if(access(token, W_OK) != 0 && access(token, F_OK) != -1){

                        char temp_buf[3000];

                        sprintf(temp_buf, "FAIL: PUT %s HTTP/1.1 --- response 403\\n\n", token);
                        sprintf(temp_buf, "%s========\n", temp_buf);

                        pthread_mutex_lock(&fileptr_lock); //lock for shared var
                        int32_t cur_fp = log_fptr;    //grab current f ptr
                        log_fptr += strlen(temp_buf);    //increment f ptr
                        pthread_mutex_unlock(&fileptr_lock);

                        int32_t fd1;
                        fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);

                        pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);


                        dprintf(clients_socket, "HTTP/1.1 403 Forbidden\r\n\r\n");
                        dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                        close(clients_socket);
                    } else {

                        //write process
            //            pthread_mutex_lock(&writer_lock); //lock mutex for writer

                        // open file
                        int32_t fd = open(token, O_WRONLY | O_CREAT | O_TRUNC, PERMISSION);

                        // move to content-length
                        for (size_t i = 0; i < THRE; i++) {
                            token = strtok(NULL, s);
                        }
                        token = strtok(NULL, "\n");
                        token = strtok(NULL, s);

                        char cont_l[] = "Content-Length:";
                        int32_t cont_len = 0;

                        char temp_buf[3000]; //EDIT TO MAX SIZE -------------------------
                        
                        // content length given
                        if (strcmp(token, cont_l) == SAME) {

                            token = strtok(NULL, s);

                            cont_len = atoi(token); // get content-length
                            
                            sprintf(temp_buf, "PUT %s length %d\n", rec_name, cont_len);

                            //calculate space needed
                            int32_t last_line = cont_len % 20;
                            int32_t num_lines = cont_len / 20;
                            int32_t total_space = (num_lines * 69) + ((last_line * 3) + 9);
                            total_space += strlen(temp_buf);
                            total_space += strlen("========\n");
                            
                            //lock when updating file pointer
                            pthread_mutex_lock(&fileptr_lock); //lock for shared var
                            int32_t cur_fp = log_fptr;   //grab current f ptr
                            log_fptr += total_space;    //increment f ptr
                            //pthread_mutex_unlock(&fileptr_lock);

                            //write the first line
                            
                            int32_t fdl = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);
                            sprintf(temp_buf, "PUT %s length %d\n", rec_name, cont_len);

                            cur_fp += pwrite(fdl, temp_buf, strlen(temp_buf), cur_fp);

                            char read_buff[ONE] = "";
                            ssize_t sz_read = NONE;
                            ssize_t sz_pasrsed = NONE;
                            int32_t counter = 0;
                            int32_t bytes_written = 0;
                            while (sz_pasrsed < cont_len) { // keep reading until EOF file
                                sz_read = read(clients_socket, read_buff, 1); 
                                write(fd, &read_buff, 1); 

                                if(counter == 0){ //write zero padded num
                                    sprintf(temp_buf, "%08d", bytes_written);
                                    cur_fp += pwrite(fdl, temp_buf, strlen(temp_buf), cur_fp);

                                }
                                //write hex number
                                sprintf(temp_buf, " %02X", (unsigned char)*read_buff);
                                cur_fp += pwrite(fdl, temp_buf, strlen(temp_buf), cur_fp);

                                counter++;

                                //add new line at end
                                if(counter == 20 || (num_lines == 0 && counter == last_line)){
                                    cur_fp += pwrite(fdl, "\n", strlen("\n"), cur_fp); 

                                    counter = 0;
                                    bytes_written += 20;
                                    num_lines--;
                                }

                                sz_pasrsed += 1;
                                read_buff[NONE] = EMPTY; // clear buffer
                                if(sz_pasrsed == cont_len){
                                    break;
                                }

                            }
                            //finish log
                            pwrite(fdl, "========\n", strlen("========\n"), cur_fp);
                            pthread_mutex_unlock(&fileptr_lock);

                        } else { // content length not specified
                            char read_buff[ONE] = "";
                            ssize_t sz_read = sizeof(char);
                            while (sz_read > NONE) { // keep reading until EOF file
                                sz_read = read(clients_socket, & read_buff, 1);
                                write(fd, & read_buff, 1);

                                cont_len++;
                                read_buff[NONE] = EMPTY; // clear buffer
                            }
                        }

                        if (check == INVALID) {
                            // file not in directory
                            dprintf(clients_socket, "HTTP/1.1 201 Created\r\n");
                            dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                        } else {
                            dprintf(clients_socket, "HTTP/1.1 200 OK\r\n");
                            dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                        }
                        close(fd);


            //            pthread_mutex_unlock(&writer_lock); //unlock mutex to allow reader
                        
                    }

                } else {
                    char temp_buf[3000];

                    sprintf(temp_buf, "FAIL: %s %s HTTP/1.1 --- response 500\\n\n", request, token);
                    sprintf(temp_buf, "%s========\n", temp_buf);

                    pthread_mutex_lock(&fileptr_lock); //lock for shared var
                    int32_t cur_fp = log_fptr;    //grab current f ptr
                    log_fptr += strlen(temp_buf);    //increment f ptr
                    pthread_mutex_unlock(&fileptr_lock);

                    int32_t fd1;
                    fd1 = open(log_file, O_WRONLY | O_CREAT | O_APPEND, PERMISSION);

                    pwrite(fd1, temp_buf, strlen(temp_buf), cur_fp);

                    dprintf(clients_socket, "HTTP/1.1 500 Internal Server Error\r\n");
                    dprintf(clients_socket, "Content-Length: %d\r\n\r\n", NONE);
                }
            }
            close(clients_socket);
            //pthread_exit(NULL);
            //return NULL;
            sem_post(&avail_threads);
        }
    }
}



int main(int argc, char *argv[]) {

    int opt;
    int num_threads = 4;
    char *host_name = NULL;
    int32_t PORT = DEFAULT_PORT;


    while((opt = getopt(argc, argv, "l:N:")) != -1){
        switch(opt){
            case 'N':
                num_threads = atoi(optarg);
                //printf("caught dash: %s\n", optval);
                break;
            case 'l':
                log_file = optarg;
                //printf("log_file: %s\n", optarg);
                break;
            default:
                continue;
                //printf("def %d\n", opt);
                //sscanf(optarg, "%d", &num_threads);
                //num_threads = atoi(optarg);
        }
    }
    //printf("dash val: %d\n", num_threads);
    //printf("log_file: %s\n", log_file);
    char arr[2][200];

    int ptr1 = 0;
    for(; optind < argc; optind++){
        strcpy(arr[ptr1], argv[optind]);
        //arr[ptr1] = optind;
        ptr1++;
        //printf("extra args: %s\n", argv[optind]);
    }
    if(ptr1 == 0){ //no address passed in
        perror("usage error");
        return ERR;
    }

    host_name = arr[0];
    if (ptr1 == 2){ //if port was specifiede
        PORT = atoi(arr[1]);
    }

    //printf("hostname: %s\n", host_name);
    //printf("port: %d\n", PORT);
    

    // create a socket
    int32_t socket_fd = socket(AF_INET, SOCK_STREAM, IP_PROTO); //socket_fd defined here
    if (socket_fd == FAILED) {
        perror("Socket failed");
        return ERR;
    }

    // defines servers address
    struct sockaddr_in serv_address;
    serv_address.sin_family = AF_INET;

    if (argc == ONE) {
        perror("usage error");
        return ERR;
    } 

    serv_address.sin_addr.s_addr = inet_addr(host_name); // INADDR_ANY;
    char local[] = "localhost";
    if (strcmp(host_name, local) == SAME) {
        serv_address.sin_addr.s_addr = INADDR_ANY;
    }

    serv_address.sin_port = htons(PORT);

    // attach socket to port 80
    int32_t bkey =
        bind(socket_fd, (struct sockaddr * ) & serv_address, sizeof(serv_address));
    if (bkey < FAILED) {
        perror("Bind error");
        return ERR;
    }

    int32_t listened = listen(socket_fd, TUNE);
    if (listened < FAILED) {
        perror("listned failed");
        return ERR;
    }

    int32_t clients_socket;
    int32_t addrlen = sizeof(serv_address); 


    sem_init(&avail_threads, 0, num_threads); //replace with num of threads
    sem_init(&assigned_threads, 0, 0); 
    pthread_mutex_init(&lock, NULL); //initialize mutex lock
    pthread_mutex_init(&writer_lock, NULL);
    pthread_mutex_init(&fileptr_lock, NULL);


    //create threads
    //pthread_t thread_ids[num_threads]; //replace with flag param from command line 
    pthread_t *thread_ids = (pthread_t *)calloc(num_threads, sizeof(pthread_t));


    //initialize all threads
    for(int i = 0; i < num_threads; i++){
        pthread_create(&thread_ids[i], NULL, handle_request, &clients_socket);
        //sem_wait(&avail_threads);
    }


    while(1){   //dispatcher 

        clients_socket = accept(socket_fd, (struct sockaddr * ) & serv_address, (socklen_t * ) & addrlen);
        printf("accepted");
        sem_wait(&avail_threads);
        sem_post(&assigned_threads);

    }


    printf("HEE");
    //create 
    //handle_request();

    pthread_join(thread_ids[0], NULL);
    return 0;
}
