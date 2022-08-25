#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <pthread.h>

#define buffer_size 20000

struct userInfo{
    char user[15];
    char user_group[15];
};

struct fileCapability{
    char file_name[20];
    struct userInfo user;
    char capability[7];
};

const char *user_group_string = "Ken|AOS-students/\nBarbie|AOS-students/\nclassmate1|AOS-students/\nuser1|CSE-students/\nuser2|CSE-students/\nuser3|CSE-students/\n";
const char *user_group_file = "server_folder/user_group.txt";
const char *capability_list_file = "server_folder/capability_list.txt";
const char server_folder_path[] = "server_folder/";

int check_capability_correction(char *capability){
    // check input capability is in correct form
    // only store read & write capability
    // correct = 1 means capability is in correct form
    // correct = 0 means capability is not correct
    int correct = 1;

    for(int i = 0; i < 6; i++){
        if((i % 2) == 0){
            // read field
            if(capability[i] != 'r' && capability[i] != '-'){
                correct = 0;
                break;
            }
        }else if((i % 2) == 1){
            // write field
            if(capability[i] != 'w' && capability[i] != '-'){
                correct = 0;
                break;
            }
        }
    }

    return correct;
}

int create_user_group(){
    // no user_group.txt file, create default one
    int fd;
    
    fd = open(user_group_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(!(write(fd, user_group_string, strlen(user_group_string)) == strlen(user_group_string))){
        perror("user_group:");
    }
    // we need to flush the file, so we close it then open it with read only
    close(fd);
    fd = open(user_group_file, O_RDONLY, 0644);

    return fd;
}

char *find_user_group(int fd, char *user){
    char *group = malloc(15 * sizeof(char));
    char *buff = malloc(buffer_size * sizeof(char));
    const char *delim1 = "/\n", *delim2 = "|";
    char *token_line, *token_word, *token_save;

    strcpy(group, "");
    read(fd, buff, buffer_size);
    if(strcmp(buff, "") == 0){
        // file has corrupted, create a new one
        // close original fd and allocate a new buff
        close(fd);
        free(buff);
        buff = malloc(buffer_size * sizeof(char));
        fd = create_user_group();
        read(fd, buff, buffer_size);
        printf("user_group.txt has corrupted, create a default user_group.txt\n");
    }
    token_line = strtok_r(buff, delim1, &token_save);
    token_word = strtok(token_line, delim2);
    if(strcmp(token_word, user) == 0){
        // first line of data is the user, get user's group
        token_word = strtok(NULL, delim2);
        strcpy(group, token_word);
    }else{
        // find the other lines
        while(token_line != NULL ){
            // use strtok_r to stare remain part of the string in token_save
            // because strtok will mess up the string
            token_line = strtok_r(NULL, delim1, &token_save);
            token_word = strtok(token_line, delim2);
            if(strcmp(token_word, user) == 0){
                // found the user, get user's group
                token_word = strtok(NULL, delim2);
                strcpy(group, token_word);
                break;
            }
        }
    }
    free(buff);

    return group;
}

char *build_capability_string(char *file_name, char *capability, struct userInfo user){
    char *temp = malloc(50 * sizeof(char));

    strcpy(temp, file_name);
    strcat(temp, "|");
    strcat(temp, user.user);
    strcat(temp, "|");
    strcat(temp, user.user_group);
    strcat(temp, "|");
    strcat(temp, capability);
    strcat(temp, "/\n");

    return temp;
}

char *cut_packet_string(char *message, int length){
    char *temp = malloc(buffer_size * sizeof(char));
    
    strncpy(temp, message, length);
    temp[length] = '\0';

    return temp;
}

int create_file(struct userInfo user){
    // create mode, format: create [file name] [capability right]
    // return value: 0 = file exist, 1 = success, -1 = format error
    int fd_create, fd_capability_list;
    char *capability_string, *token;
    char file_name[20], file_capability[10], file_path[30];

    // get file name & capability from user input
    token = strtok(NULL, " \n\0");
    strcpy(file_name, token);
    token = strtok(NULL, " \n\0");
    strcpy(file_capability, token);
    strcpy(file_path, server_folder_path);
    strcat(file_path, file_name);

    // check input capability's correction first, file existence next, finally create the file
    if(check_capability_correction(file_capability)){
        // I check file existence here, so i don't check capability_list when refreshing capability_list
        fd_create = open(file_path, O_WRONLY|O_CREAT|O_EXCL, 0644);
        if(fd_create == -1){
            close(fd_create);
            return 0;
        }else{
            // open capability_list and append a new file capability
            fd_capability_list = open(capability_list_file, O_RDWR|O_CREAT|O_APPEND, 0644);
            capability_string = build_capability_string(file_name, file_capability, user);
            write(fd_capability_list, capability_string, strlen(capability_string));
            close(fd_capability_list);
            close(fd_create);
            free(capability_string);
            return 1;
        }
    }else{
        return -1;
    }
}

int read_file(struct userInfo user, int clientfd){
    // read mode, format: read [file name]
    // return value: 0 = no file, 1 = success, 2 = file not available, -1 = permission denied
    char *buff_read = malloc(buffer_size * sizeof(char));
    char *buff_capability = malloc(buffer_size * sizeof(char));
    char *token;
    char file_name[20], file_path[30];
    const char *delim1 = "/\n", *delim2 = "|";
    char *token_line, *token_word, *token_save;
    struct fileCapability file;
    int fd_read, fd_capability_list, found = 0;
    long long int file_size;

    // get file name from user input
    token = strtok(NULL, " \n\0");
    strcpy(file_name, token);
    strcpy(file_path, server_folder_path);
    strcat(file_path, file_name);
    
    fd_capability_list = open(capability_list_file, O_RDONLY|O_CREAT);
    read(fd_capability_list, buff_capability, buffer_size);

    token_line = strtok_r(buff_capability, delim1, &token_save);
    token_word = strtok(token_line, delim2);
    if(strcmp(token_word, file_name) == 0){
        // first line of data is the file in capability_list, get the information about the file
        strcpy(file.file_name, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user_group, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.capability, token_word);
        found = 1;
    }else{
        // find the other lines
        while(token_line != NULL){
            // use strtok_r to stare remain part of the string in token_save
            // because strtok will mess up the string
            token_line = strtok_r(NULL, delim1, &token_save);
            token_word = strtok(token_line, delim2);
            if(strcmp(token_word, file_name) == 0){
                // found the file, get file's information
                strcpy(file.file_name, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user_group, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.capability, token_word);
                found = 1;
                break;
            }
        }
    }
    
    close(fd_capability_list);
    free(buff_capability);
    // if yes, continue to check capability
    // if not, print the error message
    if(found){
        if((strcmp(file.user.user, user.user) == 0 && file.capability[0] == 'r') || (strcmp(file.user.user_group, user.user_group) == 0 && file.capability[2] == 'r') || ((strcmp(file.user.user, user.user) != 0) && (strcmp(file.user.user_group, user.user_group) != 0) && file.capability[4] == 'r')){
            // check capablity, read rules are:
            // 1. File belongs to the user and the user has permission
            // 2. The user is in the same group as file owner and the group has permission
            // 3. The file can be read by others (the user is neither file owner or in the group)
            fd_read = open(file_path, O_RDONLY);
            if(flock(fd_read, LOCK_SH|LOCK_NB) == -1){
                close(fd_read);
                return 2;
            }
            send(clientfd, "Read1", strlen("Read1"), 0);
            sleep(1);

            file_size = lseek(fd_read, 0, SEEK_END);
            lseek(fd_read, 0, SEEK_SET);
            sprintf(buff_read, "%lld", file_size);
            send(clientfd, buff_read, strlen(buff_read), 0);

            size_t string_size = 0;
            while((string_size = read(fd_read, buff_read, buffer_size)) > 0){ 
                sleep(1); // sleep 1 second so that client wont miss the data
                send(clientfd, buff_read, string_size, 0);
                memset(buff_read , 0 , buffer_size);
            }
            close(fd_read);
            return 1;
        }else{
            return -1;
        }
    }else{
        return 0;
    }
}

int write_file(struct userInfo user, int clientfd){
    // write mode, format: write [file name] [overwrite/append]
    // return value: 0 = file not exist, 1 = success, 2 = file not available, -1 = permission denied
    struct fileCapability file;
    char *capability_string, *token;
    const char *delim1 = "/\n", *delim2 = "|";
    char *token_line, *token_word, *token_save;
    char *result;
    char *buff_read = malloc(buffer_size * sizeof(char));
    char *buff_capability = malloc(buffer_size * sizeof(char));
    char file_name[20], file_mode[2], file_path[30];
    char received[buffer_size];
    int fd_write, fd_capability_list, found = 0, packet_len;
    long long int packet_totoal_len;

    // get file name & capability from user input
    token = strtok(NULL, " \n\0");
    strcpy(file_name, token);
    token = strtok(NULL, " \n\0");
    strcpy(file_mode, token);
    strcpy(file_path, server_folder_path);
    strcat(file_path, file_name);
    
    fd_capability_list = open(capability_list_file, O_RDONLY);
    read(fd_capability_list, buff_capability, buffer_size);

    token_line = strtok_r(buff_capability, delim1, &token_save);
    token_word = strtok(token_line, delim2);
    if(strcmp(token_word, file_name) == 0){
        // first line of data is the file in capability_list, get the information about the file
        strcpy(file.file_name, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user_group, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.capability, token_word);
        found = 1;
    }else{
        // find the other lines
        while(token_line != NULL){
            // use strtok_r to stare remain part of the string in token_save
            // because strtok will mess up the string
            token_line = strtok_r(NULL, delim1, &token_save);
            token_word = strtok(token_line, delim2);
            if(strcmp(token_word, file_name) == 0){
                // found the file, get file's information
                strcpy(file.file_name, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user_group, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.capability, token_word);
                found = 1;
                break;
            }
        }
    }
    
    close(fd_capability_list);
    // if yes, continue to check capability
    // if not, print the error message
    if(found){
        if((strcmp(file.user.user, user.user) == 0 && file.capability[1] == 'w') || (strcmp(file.user.user_group, user.user_group) == 0 && file.capability[3] == 'w') || ((strcmp(file.user.user, user.user) != 0) && (strcmp(file.user.user_group, user.user_group) != 0) && file.capability[5] == 'w')){
            // check capablity, read rules are:
            // 1. File belongs to the user and the user has permission
            // 2. The user is in the same group as file owner and the group has permission
            // 3. The file can be written by others (the user is neither file owner or in the group)
            if(strcmp(file_mode, "o") == 0){
                fd_write = open(file_path, O_WRONLY); // don't open with O_TRUNC, otherwise it will clean all the content
                if(flock(fd_write, LOCK_EX|LOCK_NB) == -1){
                    close(fd_write);
                    return 2;
                }else{
                    close(fd_write);
                    fd_write = open(file_path, O_WRONLY|O_TRUNC);
                }
            }else if(strcmp(file_mode, "a") == 0){
                fd_write = open(file_path, O_WRONLY|O_APPEND);
                if(flock(fd_write, LOCK_EX|LOCK_NB) == -1){
                    close(fd_write);
                    return 2;
                }
            }
            send(clientfd, "Transfer", strlen("Transfer"), 0);
            packet_len = recv(clientfd, received, buffer_size, 0);
            result = cut_packet_string(received, packet_len);
            packet_totoal_len = atoi(result);
            for(long long int i = 0; i < packet_totoal_len;){
                packet_len = recv(clientfd, received, buffer_size, 0);
                result = cut_packet_string(received, packet_len);
                i += packet_len;
                write(fd_write, result, strlen(result));
            }
            close(fd_write);
            free(result);
            return 1;
        }else{
            return -1;
        }
    }else{
        return 0;
    }

}

int change_file_mode(struct userInfo user){
    // change mode, format: changemode [file name] [capability right]
    // return 0 = no file, 1 = success, 2 = not file owner, -1 = format error
    int fd_capability_list, found = 0;
    char *buff_capability = malloc(buffer_size * sizeof(char));
    char *new_capability_string = malloc(buffer_size * sizeof(char));
    char *token;
    char file_name[20], file_capability[10], temp_capability_string[50];
    const char *delim1 = "/\n", *delim2 = "|";
    char *token_line, *token_word, *token_save;
    struct fileCapability file;

    // get file name from user input
    token = strtok(NULL, " \n\0");
    strcpy(file_name, token);
    token = strtok(NULL, " \n\0");
    strcpy(file_capability, token);
    if(!check_capability_correction(file_capability)){
        return -1;
    }
    
    fd_capability_list = open(capability_list_file, O_RDONLY);
    read(fd_capability_list, buff_capability, buffer_size);
    // initialize new string
    strcpy(new_capability_string, "");

    token_line = strtok_r(buff_capability, delim1, &token_save);
    // save the entire line for later use
    strcpy(temp_capability_string, token_line);
    token_word = strtok(token_line, delim2);
    if(strcmp(token_word, file_name) == 0){
        // first line of data is the file, get the information about the file
        strcpy(file.file_name, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user, token_word);
        token_word = strtok(NULL, delim2);
        strcpy(file.user.user_group, token_word);
        strcpy(file.capability, file_capability);
        strcat(new_capability_string, build_capability_string(file.file_name, file.capability, file.user));
        found = 1;
        while(1){
            // use strtok_r to stare remain part of the string in token_save
            // because strtok will mess up the string
            token_line = strtok_r(NULL, delim1, &token_save);
            if(token_line == NULL){
                break;
            }
            strcpy(temp_capability_string, token_line);
            strcat(new_capability_string, temp_capability_string);
            strcat(new_capability_string, "/\n");
        }
    }else{
        strcat(new_capability_string, temp_capability_string);
        strcat(new_capability_string, "/\n");
        // find the other lines
        while(1){
            // use strtok_r to stare remain part of the string in token_save
            // because strtok will mess up the string
            token_line = strtok_r(NULL, delim1, &token_save);
            if(token_line == NULL){
                break;
            }
            strcpy(temp_capability_string, token_line);
            token_word = strtok(token_line, delim2);
            if(strcmp(token_word, file_name) == 0){
                // found the file, get file's information
                strcpy(file.file_name, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user, token_word);
                token_word = strtok(NULL, delim2);
                strcpy(file.user.user_group, token_word);
                strcpy(file.capability, file_capability);
                strcat(new_capability_string, build_capability_string(file.file_name, file.capability, file.user));
                found = 1;
            }else{
                strcat(new_capability_string, temp_capability_string);
                strcat(new_capability_string, "/\n");
            }
        }
    }

    free(buff_capability);
    if(found){
        // found the file, then check file owner
        // rewrite capability_list.txt
        if(strcmp(file.user.user_group, user.user_group) == 0){
            // close file descriptor first and then open it with new parameter
            close(fd_capability_list);
            fd_capability_list = open(capability_list_file, O_RDWR|O_TRUNC);
            write(fd_capability_list, new_capability_string, strlen(new_capability_string));
            close(fd_capability_list);
            free(new_capability_string);
            return 1;
        }else{
            close(fd_capability_list);
            free(new_capability_string);
            return 2;
        }
    }else{
        close(fd_capability_list);
        free(new_capability_string);
        return 0;
    }
}

void *serve_client(void *arg){
    struct userInfo user;
    struct stat st = {0};
    char input[50];
    char received[buffer_size];
    char *token;
    char *result, *file_data = malloc(buffer_size * sizeof(char));
    int packet_len, err_code;
    int fd;
    int clientfd = (intptr_t)arg;

    // create server folder
    if(stat(server_folder_path, &st) == -1) {
        mkdir(server_folder_path, 0700);
    }
    // initialize user_group to empty
    strcpy(user.user_group, "");

    // client user login
    while(1){
        packet_len = recv(clientfd, received, buffer_size, 0);
        result = cut_packet_string(received, packet_len);
        strcpy(user.user, result);
        free(result);

        // open user_group.txt to find user's group
        if((fd = open(user_group_file, O_RDONLY, 0644)) == -1){
            // No file, create a default user_group file
            printf("No user_group file, creating default file\n");
            fd = create_user_group();
        }
        // get user's group
        strcpy(user.user_group, find_user_group(fd, user.user));
        if(strcmp(user.user_group, "") == 0){
            send(clientfd, "Fail", strlen("Fail"), 0);
        }else{
            // end of finding user's group
            send(clientfd, "Success", strlen("Success"), 0);
            close(fd);
            break;
        }
        close(fd);
    }
    // start user action
    while(1){
        // checked user, start the command section
        packet_len = recv(clientfd, received, buffer_size, 0);
        result = cut_packet_string(received, packet_len);
        if(strcmp(result, "exit") == 0){
            //free(clientfd);
            //return NULL;
            break;
        }else if(strcmp(result, "error") == 0){
            int a = 10 / 0;
        }
        token = strtok(result, " \n\0");
        if(strcmp(token, "create") == 0){
            err_code = create_file(user);
            if(err_code == -1){
                send(clientfd, "Create-1", strlen("Create-1"), 0);
            }else if(err_code == 0){
                send(clientfd, "Create0", strlen("Create0"), 0);
            }else if(err_code == 1){
                send(clientfd, "Create1", strlen("Create1"), 0);
            }
        }else if(strcmp(token, "read") == 0){
            err_code = read_file(user, clientfd);
            if(err_code == -1){
                send(clientfd, "Read-1", strlen("Read-1"), 0);
            }else if(err_code == 0){
                send(clientfd, "Read0", strlen("Read0"), 0);
            }else if(err_code == 2){
                send(clientfd, "Read2", strlen("Read2"), 0);
            }else if(err_code == 1){
                // if you want to do something with a successful action
            }
        }else if(strcmp(token, "write") == 0){
            err_code = write_file(user, clientfd);
            if(err_code == -1){
                send(clientfd, "Write-1", strlen("Write-1"), 0);
            }else if(err_code == 0){
                send(clientfd, "Write0", strlen("Write0"), 0);
            }else if(err_code == 1){
                send(clientfd, "Write1", strlen("Write1"), 0);
            }else if(err_code == 2){
                send(clientfd, "Write2", strlen("Write2"), 0);
            }
        }else if(strcmp(token, "changemode") == 0){
            err_code = change_file_mode(user);
            if(err_code == -1){
                send(clientfd, "Change-1", strlen("Change-1"), 0);
            }else if(err_code == 0){
                send(clientfd, "Change0", strlen("Change0"), 0);
            }else if(err_code == 1){
                send(clientfd, "Change1", strlen("Change1"), 0);
            }else if(err_code == 2){
                send(clientfd, "Change2", strlen("Change2"), 0);
            }
        }else{
            send(clientfd, "Unknown0", strlen("Unknown0"), 0);
        }
    }
}

void main(int argc, char *argv[]){
    struct sockaddr_in info, client_info;
    int sockfd, client_number = 0;
    int clientfd;
    pid_t pid;
    pthread_t threads[10];
    int rc, counter = 0;

    signal(SIGCHLD,SIG_IGN);
    bzero(&info,sizeof(info));//初始化，將struct涵蓋的bits設為0
    info.sin_family = AF_INET;//sockaddr_in為Ipv4結構
    info.sin_addr.s_addr = inet_addr("127.0.0.1");//IP address
    info.sin_port = htons(9090);

    /* open socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    /* bind socket to a name */
    if(bind(sockfd, (struct sockaddr *)&info, sizeof(info))){
        perror("bind");
        exit(1);
    }
    /* prepare to receive multiple connect requests */
    if(listen(sockfd, 128)){
        perror("listen");
        exit(1);
    }
    while(1){
        clientfd = accept(sockfd, (struct sockaddr *)&client_info, (socklen_t *)&client_number);
        rc = pthread_create(&threads[counter], NULL, serve_client, (void *)(intptr_t)clientfd);
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
        pthread_detach(threads[counter]);
        counter++;
    }
}