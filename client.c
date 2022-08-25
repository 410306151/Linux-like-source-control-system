#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>

#define buffer_size 20000

struct userInfo{
    char user[15];
    char user_group[15];
};

const char *user_group_string = "Ken|AOS-students/\nBarbie|AOS-students/\nclassmate1|AOS-students/\nuser1|CSE-students/\nuser2|CSE-students/\nuser3|CSE-students/\n";
const char *user_group_file = "user_group.txt";
const char *capability_list = "capability_list.txt";

char *cut_packet_string(char *message, int length){
    char *temp = malloc(buffer_size * sizeof(char));
    
    strncpy(temp, message, length);
    temp[length] = '\0';

    return temp;
}

void main(int argc, char *argv[]){
    struct sockaddr_in info, client_info;
    struct hostent *host;
    char message[100], test[100], received[100], *result, data[buffer_size];
    char *token, file_name[10], exec_mode[15], file_mode[2];
    char buffer[buffer_size];
    int sockfd, packet_len;
    int fd;
    long long int packet_totoal_len, file_size;

    bzero(&info,sizeof(info));//初始化，將struct涵蓋的bits設為0
    info.sin_family = AF_INET;//sockaddr_in為Ipv4結構
    info.sin_addr.s_addr = inet_addr("127.0.0.1");//IP address
    info.sin_port = htons(9090);

    /* open socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* bind socket to a name */
    if (connect(sockfd, (struct sockaddr *)&info, sizeof(info)) == -1) {
        perror("connect");
        exit(1);
    }

    // try to log in
    while(1){
        printf("User name: ");
        fgets(message, 100, stdin);
        message[strcspn(message, "\n")] = '\0'; // convert \n to \0, because fgest will get string like: string\n\0
        // exit the programe
        if(strcmp(message, "exit") == 0){
            close(sockfd);
            exit(0);
            break;
        }
        send(sockfd, message, strlen(message), 0);
        packet_len = recv(sockfd, received, 100, 0);
        result = cut_packet_string(received, packet_len);
        if(strcmp(result, "Success") == 0){
            printf("Log In!\n");
            free(result);
            break;
        }else{
            printf("Fail\n");
        }
        free(result);
    }
    while(1){
        printf("What is your order? ");
        fgets(message, 100, stdin);
        message[strcspn(message, "\n")] = '\0'; // convert \n to \0, because fgest will get string like: string\n\0

        strcpy(test, message);
        // get file name
        token = strtok(test, " ");
        strcpy(exec_mode, token);
        if(strcmp(exec_mode, "write") == 0){
            token = strtok(NULL, " ");
            strcpy(file_name, token);
            // start transfering data
            fd = open(file_name, O_RDONLY);
            if(fd == -1){
                printf("File doesn't exist\n");
                continue;
            }
            token = strtok(NULL, " ");
            strcpy(file_mode, token);
            if(strcmp(file_mode, "o") != 0 && strcmp(file_mode, "a") != 0){
                printf("write mode eror\n");
                close(fd);
                continue;
            }
        }

        send(sockfd, message, strlen(message), 0);
        // exit the program
        printf("%s\n", message);
        if(strcmp(message, "exit") == 0){
            close(sockfd);
            exit(0);
            break;
        }
        packet_len = recv(sockfd, received, 100, 0);
        result = cut_packet_string(received, packet_len);
        // handle return error message
        if(strcmp(result, "Create-1") == 0 || strcmp(result, "Change-1") == 0){
            printf("Capability format is incorrect. E.g., rwr---\n");
        }else if(strcmp(result, "Create0") == 0){
            printf("File exist\n");
        }else if(strcmp(result, "Create1") == 0){
            printf("File has been created\n");
        }else if(strcmp(result, "Read-1") == 0){
            printf("You don't have permission to read the file\n");
        }else if(strcmp(result, "Read0") == 0){
            printf("No such file to read\n");
        }else if(strcmp(result, "Read1") == 0){
            // print the data
            packet_len = recv(sockfd, data, buffer_size, 0);
            result = cut_packet_string(data, packet_len);

            packet_totoal_len = atoi(result);
            for(long long int i = 0; i < packet_totoal_len;){
                packet_len = recv(sockfd, received, buffer_size, 0);
                result = cut_packet_string(received, packet_len);
                printf("%s", result);
                i += packet_len;
            }
        }else if(strcmp(result, "Change0") == 0 || strcmp(result, "Write0") == 0){
            printf("No such file\n");
        }else if(strcmp(result, "Change1") == 0){
            printf("File mode changed successfully\n");
        }else if(strcmp(result, "Change2") == 0){
            printf("You are not file owner\n");
        }else if(strcmp(result, "Write-1") == 0){
            printf("You don't have permission to write the file\n");
        }else if(strcmp(result, "Write2") == 0 || strcmp(result, "Read2") == 0){
            printf("File is not available now\n");
        }else if(strcmp(result, "Transfer") == 0){
            file_size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            sprintf(buffer, "%lld", file_size);
            send(sockfd, buffer, strlen(buffer), 0);

            size_t string_size = 0;
            while((string_size = read(fd, buffer, buffer_size)) > 0){ 
                sleep(10);
                send(sockfd, buffer, string_size, 0);
                memset(buffer , 0 , buffer_size);
            }
            packet_len = recv(sockfd, received, 100, 0);
            result = cut_packet_string(received, packet_len);
            if(strcmp(result, "Write1") == 0){
                printf("File transfer success\n");
            }else{
                printf("File transfer error\n");
            }
        }else if(strcmp(result, "Unknown0") == 0){
            printf("Unknown cammand\n");
        }
    }
    exit(0);
}