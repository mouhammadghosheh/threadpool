//
// Created by mouha on 30/12/2022.
//
#include "threadpool.h"
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define VERSION "HTTP/1.1"


#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

//function initial
char *get_mime_type(char *name);

void build_response(int, char *, char *, char *, char *, int, char *, char *, int);

int read_request(int sd);

int get_index(char *str);

void socketConnect(int, int, int);

int main(int argc, char *argv[]) {
    //check if arguments are faulty
    if (argc < 4) {
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    } else if (argc > 4) {
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    }
    //assign numbers to integers
    int port = atoi(argv[1]);
    int pool_size = atoi(argv[2]);
    int Max_From_server = atoi(argv[3]);
    if(port<0||pool_size<0||Max_From_server<0){
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        exit(0);
    }
    socketConnect(port, pool_size, Max_From_server);
}

void socketConnect(int port, int pool_size, int Max_From_server) {
    //connection structs initialization
    int sd;
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

//socket innit
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }
    int bin;
    //bind
    bin = bind(sd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (bin < 0) {
        perror("bind");
        exit(1);
    }
    //create threadpool
    threadpool *pool = create_threadpool(pool_size);
    if(pool == NULL) {
        perror("threadpool");
        exit(1);
    }
    //listen to connections from servers
    int lis = listen(sd, Max_From_server);
    if (lis == -1) {
        perror("listen");
        exit(1);
    }
    int i = 0;
    intptr_t connection[Max_From_server];
    //check until max number of requests is reached
    while (i < Max_From_server) {

        int clientLen = sizeof(serv_addr);

        if ((connection[i] = accept(sd, (struct sockaddr *) &serv_addr, (socklen_t*)&clientLen)) < 0) {
            perror("accept");
        } else {
            dispatch(pool, (dispatch_fn) read_request, (void *) connection[i]);
        }

        i++;
    }
//destroy threadpool and close socket
    destroy_threadpool(pool);
    close(sd);


}


int read_request(int sd) {
    //usefull inits
    char request[1024] = {0};
    memset(request, 0, sizeof(request));
    int reading;
    int status;
    char *phrase;
    time_t now;
    char timebuf[128] = { 0};
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

//read the request into the request buffer
    while ((reading = read(sd, request, sizeof(request))) >= 0) {
        if (reading == -1) {
            perror("read");
            exit(1);
        }
        //stop at \r\n
        if (reading > 0) {
            request[get_index(request)] = '\0';
            break;
        }
    }
    //check if the requests contains of 2 spaces, so we make sure it is not a faulty response
    int i = 0, space_count = 0;
    while (i < strlen(request)) {
        if (request[i] == ' ')
            space_count++;
        i++;
    }
    char *method = strtok(request, " ");
    char *path = strtok(NULL, " ");
    char *protocol = strtok(NULL, " ");

    //check request for error 400
    if (method == NULL || path == NULL || protocol == NULL || space_count != 2 || strcmp(protocol, "HTTP/1.0") != 0) {

            status = 400;
            phrase = "Bad Request";
            build_response(status, phrase, timebuf, NULL, path, 113, "400 Bad Request", "Bad Request.", sd);
            return status;

    }
    //check for error 501
    if (strcmp(method, "GET") != 0) {
        status = 501;
        phrase = "Not supported";
        build_response(status, phrase, timebuf, NULL, path, 129, "501 Not supported", "Method is not supported.", sd);
        return status;
    }

    //path2 for path that can be entered
    char path2[1024] = {0};
    memset(path2, 0, sizeof(path2));
    path2[0] = '.';

    strcat(path2, path);
    DIR *dir = opendir(path2);
    struct stat file_stats;



    //if it is a file or no permissions:
    if (dir == NULL) {
//temp is stores every directory from the beginning till the end running on it slash slash
        char *temp = (char *) malloc(sizeof(char) * 1024);
        if(temp == NULL){
           status = 500;
           phrase = "Internal Server Error.";
            build_response(status, phrase,timebuf,NULL,path2,144,phrase,"Some server side error",sd);

        }
        memset(temp, 0, sizeof(char) * 1024);

        i = 0;
        temp[1] = '/';
        while (i < strlen(path2)) {
            while (path2[i] != '/' && i < strlen(path2)) {
                temp[i] = path2[i];
                i++;
            }
            //if not found
            if (stat(temp, &file_stats) < 0) {

                status = 404;
                phrase = "Not Found";
                build_response(status, phrase, timebuf, NULL, path2, 112, "404 Not Found", "File not found.", sd);
                break;
            }
                //if it is a file then check if the file has read permissions
            else if (S_ISREG(file_stats.st_mode)) {
                if (!((file_stats.st_mode & S_IROTH) && (file_stats.st_mode & S_IRGRP) &&
                    (file_stats.st_mode & S_IRUSR))) {


                    status = 403;
                    phrase = "Forbidden";

                    build_response(status, phrase, timebuf, NULL, path2, 111, "403 Forbidden", "Access denied.", sd);

                    break;
                }
                else{
                                        status = 200;
                    phrase = "OK";
                    char lastMod[128] = {0};
                    strftime(lastMod, sizeof(lastMod), RFC1123FMT, gmtime(&file_stats.st_mtime));
                    build_response(status, phrase, timebuf, lastMod, temp, (int) file_stats.st_size, NULL, NULL, sd);
                    break;
                }
                //check if path is a directory then check if it has read permissions
            } else if (S_ISDIR(file_stats.st_mode)) {
             if ((!(file_stats.st_mode & S_IXOTH))) {

                    status = 403;
                    phrase = "Forbidden";

                    build_response(status, phrase, timebuf, NULL, path2, 111, "403 Forbidden", "Access denied.", sd);

                    break;
                }
            }

            temp[i] = '/';
            i++;
        }
        free(temp);
    }
    //if directory is found
    else {
        //check if it has leading slash or not
        if (path2[strlen(path2) - 1] != '/') {
            status = 302;
            build_response(status, "Found", timebuf, NULL, path2, 123, NULL, NULL, sd);
            return 1;
        } else {
            //check if it has index.html
            struct stat file_stats2;
            char path3[1024] = {0};
            strcpy(path3, path2);
            strcat(path2, "index.html");
            int s = stat(path2, &file_stats2);

            //check if file is found and it is a file
            if (s >= 0 && S_ISREG(file_stats2.st_mode)) {
//                if ((file_stats2.st_mode & S_IROTH) && (file_stats2.st_mode & S_IRGRP) &&
//                    (file_stats2.st_mode & S_IRUSR)) {
                    status = 200;
                    phrase = "OK";
                    char lastMod[128] = {0};
                    memset(lastMod, 0, 128);
                    strftime(lastMod, sizeof(lastMod), RFC1123FMT, gmtime(&file_stats.st_mtime));
                    build_response(status, phrase, timebuf, lastMod, path2, (int) file_stats2.st_size, NULL, NULL, sd);
                    memset(path2, 0, sizeof(path2));
                //}
            } else {

                //if directory found build the html table for dir content
                struct stat fold;
                stat(path3, &fold);
                char lastMod2[128] = {0};
                strftime(lastMod2, sizeof(lastMod2), RFC1123FMT, gmtime(&fold.st_mtime));
                char header[4096] = {0};//priint header
                sprintf(header,
                        "HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\ncontent-length: %d\r\nLast-Modified: %s\r\nConnection: Close\r\n\r\n",
                        timebuf,(int)file_stats2.st_size, lastMod2);
                if((write(sd, header, sizeof(header)))==-1){
                    perror("Write");
                    exit(1);
                }
                struct stat st;
                struct dirent *dirent;
                char body[4096] = {0};
                //print html
                sprintf(body,
                        "<html><head><title>Index of %s</title></head>\n<body><h4>Index of %s</h4>\n<table CELLSPACING=8>\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n",
                        path3, path3);
                //write html to socket
                if(((write(sd, body, sizeof(body))))==-1){
                    perror("Write");
                    exit(1);
                }
                //while loop to read directory content and add the content to the html header
                while ((dirent = readdir(dir)) != NULL) {
                    bzero(body, sizeof(body));
                    char table_temp[strlen(path3) + strlen(dirent->d_name) + 1];
                    bzero(table_temp, sizeof(table_temp));
                    strcat(table_temp, path3);
                    strcat(table_temp, dirent->d_name);

                    stat(table_temp, &st);
                    bzero(timebuf, sizeof(timebuf));
                    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&st.st_mtime));

                    if (!S_ISDIR(st.st_mode)) {
                        sprintf(body, "<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%d</td></tr>\n", dirent->d_name,
                                dirent->d_name, timebuf, (int) st.st_size);
                    } else {
                        //strcat(table_temp, "/");
                        sprintf(body, "<tr><td><A HREF=\"%s/\">%s</A></td><td>%s</td><td>\n", dirent->d_name,
                                dirent->d_name, timebuf);
                    }
                    if(((write(sd, body, strlen(body)))==-1)) {
                        perror("Write");
                        exit(1);
                    }

                }
                //last body to html
                sprintf(body, "</table>\n\n<HR>\n\n<ADDRESS>webserver/1.0</ADDRESS>\n\n</body></html>");
                if(((write(sd, body, strlen(body)))==-1)) {
                    perror("Write");
                    exit(1);
                }

            }
        }
    }
    //closer directory in the end
    closedir(dir);
    return 0;
}

//get the index of the \r\n
int get_index(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == '\r' && str[i + 1] == '\n') {
            return i;
        }
    }
    return -1;
}
//function responsible for building the response
void
build_response(int status, char *phrase, char *time, char *last_mod, char *path, int length, char *body, char *last,
               int socket) {
    char response[1024];
    memset(response, 0, sizeof(response));
    //for these status
    if (status == 404 || status == 501 || status == 400 || status == 403||status == 500) {
        sprintf(response, "%s %d %s\r\n" //version - status - phrase
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n" //time
                          "Content-Type: %s\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n"
                          "<BODY><H4>%s</H4>\r\n"
                          "%s\r\n"
                          "</BODY></HTML>"
                          "\r\n", VERSION, status, phrase, time, "text/html", length, body, body, last);

    }
    //if status is 200
    else if (status == 200) {
        sprintf(response, "%s %d %s\r\n"
                          "Server: webserver/1.0\r\n"
                          "%s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %d\r\n"
                          "%s\r\n"
                          "Connection: close\r\n"
                          "\r\n", VERSION, status, phrase, time, get_mime_type(path), length, last_mod);

    } else if (status == 302) {
        sprintf(response, "%s %d %s\r\n"
                          "Server: webserver/1.0\r\n"
                          "Date: %s\r\n"
                          "Location: %s/\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n"
                          "<BODY><H4>302 Found</H4>\r\n"
                          "Directories must end with a slash.\r\n"
                          "</BODY></HTML>", VERSION, status, phrase, time,path, "text/html", length);


    }

    if((write(socket, response, strlen(response)))<0){
        perror("Write");
        exit(1);
    }

    if (status == 200) {
        //if it is a file open it then read the content then write it to the socket
        int f = open(path, O_RDONLY);
        if (f == -1){
            perror("open");
            exit(1);
        }
        unsigned char file_read[4096] = {0};
        size_t r;
        while ((r = read(f, file_read, sizeof(file_read))) > 0) {
            if (write(socket, file_read, r) < 0) {
                perror("Write");
                exit(1);
            }
            memset(file_read,0,sizeof(file_read));
        }
        close(f);
    }
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}