# threadpool
an http server with threads
Student's Name : Mohammad Ghosheh
HTTP server EX3
server.c the file has the main function and all the other usefull functions
threadpool.c the file has the threadpool definition and functionality

==Description==
This program is a HTTP server, it recieves in the main a port,pool size and maximum number of requests to handle, the program handles a request sent from the local host and parses it into strings, then we check if the request is faulty and send appropriate errors for every failiure,if the request is good then we work on the path and check it, if it is an available path on the server directory and it has read permissions, if not, the server returns a response to the client with header with appropriate information then a html body to specify the error.
if the path is a directory it displays the directory content through an html table.
the requests are handeled by threads that are in a queue and defined in the threadpool.c, every thread handles a request and is freed when it's work is done, multiple threads can be assigned to handle the requests.

==Functions==
int get_index(char *str): this function returns the index of the \r\n so we know when the end of the first line in the request header.

void socketConnect(int, int, int): this function initilizes the server struct and assigns that we want to use ipv4, then it fills the struct with the appropriate information like port,ipv4 and address, then we initilize the socket to tcp protocol  and bind the socket to the server then we create the threadpool and start listening to connections from the client, while number of requests is not above max number we wait for a request and dispatch it to a thread then call the read request function.

int read_request(int sd): this function reads the request from the socket and assigns the method,path and protocol to strings, then we check if the request is bad and we return appropriate errors, also this function goes to the path directory and checks if the path is a directory or not, if it is a file and it has read permissions, return the file to the client via build_response function, if the file is not found it also returns 404 not found, it also checks the path one directory each in it, so we can know if all the filders in the way have read permissions.
if there are read permissions and it is a directory, we check if it has index.html, if not, it returns all the directories in that directory in the form of a html table that has all the folders as well as their information like size and last modified date then write this to the socket again.

void build_response(int, char *, char *, char *, char *, int, char *, char *, int): this function gets the error status,header phrase,current time, last modification,current path, content length, html body and the small message in the end of the html and the socket. this function builds the response according to the error number, and sends it to the socket for the client, if the error is 200(file found) we read the contents of the file and write to the socket in a while loop until we read all the file then we close the file descriptor.

char *get_mime_type(char *name): this function gets a file path and checks it's extention and format, and returns the answer accordingly for the content type header

==Program Files==
server.c
threadpool.c


==How to compile==
gcc -o server server.c threadpool.c -lpthread -Wall
Run : ./server <port> <pool_size> <max_number_of_requests>
Valgrind: valgrind ./server <port> <pool_size> <max_number_of_requests>
==Input==
in the argv, the input is: <port> <pool_size> <max_number_of_requests>
respectively

==Output==
