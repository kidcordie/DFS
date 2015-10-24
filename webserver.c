/*
 * echoserver.c - A sequential echo server
 */

#include "nethelp.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

//Default values read from config file are stored as globals so they can be utilized by all threads
char root[100], contentTypes[100], defaultPages[100], portNum[10];
void *thread(void *vargp);
int readConfig(int configfd);
void parseRequest(int connfd);
void transmitMessage(int connfd, char* root, char* contentTypes, char* requestedURI, char* HTTPType, int keepAlive);
int parse(int counter, char * buf, char * result);
int parseTilNewline(int counter, char * buf, char * result);

int main(int argc, char **argv)
{
    int listenfd, connfd, port, clientlen=sizeof(struct sockaddr_in);
    int * connfdp;
    int configFD;
    struct sockaddr_in clientaddr;
    char* filename;
    pthread_t tid;
    if (argc != 2) {
	     fprintf(stderr, "usage: %s <conf File Location>\n", argv[0]);
       exit(0);
    }
    filename = argv[1];
    configFD = open(filename, O_RDONLY);
    if (configFD < 0){
      fprintf(stderr, "Bad filename\n");
      exit(0);
    }
    //if (!readConfig(configFD, portNum, root, contentTypes, defaultPages)){
    if (!readConfig(configFD)){
      fprintf(stderr, "conf file does not have port, document root, defaultPages, or conent types\n");
      exit(0);
    }
    printf("port: %s, root: %s, contentTypes: %s, defaultPages: %s\n", portNum, root, contentTypes, defaultPages);
    port = atoi(portNum);
    listenfd = open_listenfd(port);
    while (1) {
      connfdp = malloc(sizeof(int));
      *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
      pthread_create(&tid, NULL, thread, connfdp);
    }
}

/*
  Function to create a unique thread that runs parseRequest() and then closes the socket
*/
void * thread(void * vargp)
{
    int connfd = *((int *)vargp);
    //pthread_detach(pthread_self());
    free(vargp);
    parseRequest(connfd);
    close(connfd);
    return NULL;
}

/*Parses the incoming request form the socket
  If a GET request is found this function calls transmitMessage()
  To write the headers and file to the server
  If told to keep connection alive it is impolemented with a sleep timer
  and a counter that is incremented till 10
*/
void parseRequest(int connfd)
{
    int n;
    char buf[MAXLINE];
    char * requestType;
    char HTTPType[MAXLINE];
    char * requestedURI;
    int counter = 0, i, sizeofMessage;
    int timer = 10, keepAlive = 0, newRequest = 0;
    int flags = fcntl(connfd, F_GETFL, 0);
    fcntl(connfd, F_SETFL, flags | O_NONBLOCK);
    do{
      newRequest = 0;
      while((n = readline(connfd, buf, MAXLINE)) > 0) {
        timer = 10;
        printf("server received %d bytes\n", n);
        printf("buf reads %s", buf);
        counter = 0;
        char* firstWord = malloc(1 + strlen(buf));
        counter = parse(counter, buf, firstWord);
        if (strstr(firstWord, "GET") != NULL){
          requestType = firstWord;
          newRequest = 1;
          requestedURI = malloc(1 + strlen(buf));
          counter = parse(counter, buf, requestedURI);
          counter = parse(counter, buf, HTTPType);
          if (strstr(requestedURI, "\\") != NULL || strstr(requestedURI, "..") != NULL){
            sizeofMessage = 1 + strlen("HTTP/1.1 400 Bad Request: Invalid URI: ") + strlen(requestedURI) + strlen("\n");
            char * errorcode = (char*) malloc(sizeofMessage);
            strcpy(errorcode, "HTTP/1.1 400 Bad Request: Invalid URI: ");
            strcat(errorcode, requestedURI);
            strcat(errorcode, "\n");
            write(connfd, errorcode, sizeofMessage);
          }
          else if (strstr(HTTPType, "HTTP/1.1") == NULL){
            sizeofMessage = 1 + strlen("HTTP/1.1 400 Bad Request: Invalid HTTP-Version: ") + strlen(HTTPType) + strlen("\n");
            char * errorcode = (char*) malloc(sizeofMessage);
            strcpy(errorcode, "HTTP/1.1 400 Bad Request: Invalid HTTP-Version: ");
            strcat(errorcode, HTTPType);
            strcat(errorcode, "\n");
            write(connfd, errorcode, sizeofMessage);
          }
        }
        else if (!newRequest){
          sizeofMessage = 1 + strlen("HTTP/1.1 400 Bad Request: Invalid Method: ") + strlen(firstWord) + strlen("\n");
          char * errorcode = (char*) malloc(sizeofMessage);
          strcpy(errorcode, "HTTP/1.1 400 Bad Request: Invalid Method: ");
          strcat(errorcode, firstWord);
          strcat(errorcode, "\n");
          write(connfd, errorcode, sizeofMessage);
        }
        if (strstr(firstWord, "Connection")){
          char * survive = (char*) malloc(strlen(buf));
          parse(counter, buf, survive);
          if (strstr(survive, "keep-alive")){
            keepAlive = 1;
          }
        }
        //This if statement is just incase the socket is still connected but the readline function blocks on a read
        if (n < 3 && newRequest){
          transmitMessage(connfd, root, contentTypes, requestedURI, HTTPType, keepAlive);
          newRequest = 0;
        }
      }
      if (newRequest){
        transmitMessage(connfd, root, contentTypes, requestedURI, HTTPType, keepAlive);
      }
      sleep(1);
      timer--;
  }while((timer > 0) && keepAlive);
}

/*
Transmit the relevant headers and file to the server
It will also transmit error messages if they are detected
*/
void transmitMessage(int connfd, char* root, char* contentTypes, char* requestedURI, char* HTTPType, int keepAlive){
  int currentfd, fileSize;
  struct stat st;
  int sizeofMessage = 0, i=0;
  int counter, correctDocument = 0;
  if (strlen(requestedURI) > 0){
    char * requestedDocument = (char *) malloc(1 + strlen(root)+ strlen(requestedURI) );
    strcpy(requestedDocument, root);
    strcat(requestedDocument, strtok(requestedURI, "\t\n"));
    if(strstr(requestedURI, ".")){
      char * p = strtok(requestedURI, ".\t");
      p = strtok(NULL, ".\t");
      if(strstr(contentTypes, p) == NULL){
        sizeofMessage = 1 + strlen(HTTPType) + strlen(" 501 Not Implemented: ") + strlen(requestedURI) + strlen("\n");
        char * errorcode = (char*) malloc(sizeofMessage);
        strcpy(errorcode, HTTPType);
        strcat(errorcode, " 501 Not Implemented: ");
        strcat(errorcode, requestedURI);
        strcat(errorcode, "\n");
        write(connfd, errorcode, sizeofMessage);
      }
      else{
        char * filetype = (char *) malloc(2 + strlen(p));
        strcpy(filetype, ".");
        strcat(filetype, p);
        char * dummycontentTypes = (char *) malloc(1+strlen(contentTypes));
        strcpy(dummycontentTypes, contentTypes);
        char * fileinfo = strtok(dummycontentTypes, " ,");
        int found = 0;
        while(!found){
          if(strstr(fileinfo, filetype) != NULL){
            fileinfo = strtok(NULL, " ,");
            found = 1;
          }
          else{
            fileinfo = strtok(NULL, " ,");
          }
        }
        currentfd = open(requestedDocument, O_RDONLY);
        if(currentfd > 0){
          stat(requestedDocument, &st);
          fileSize = st.st_size;
          char fileSizeStr[15];
          char * keepAliveStr = (char*)malloc(strlen("Connection: keep-alive\n"));
          if(keepAlive){
            strcpy(keepAliveStr, "Connection: keep-alive\n");
          }
          sprintf(fileSizeStr, "%d", fileSize);
          sizeofMessage = 1 + strlen(HTTPType) + strlen(" 200 Document Follows\n") + strlen("Content-Type: ") +
          strlen(fileinfo) + strlen("\n") + strlen("Content-Length ") + strlen(fileSizeStr) + strlen("\n") + strlen(keepAliveStr);
          char * statuscode = (char*) malloc(sizeofMessage);
          strcpy(statuscode, HTTPType);
          strcat(statuscode, " 200 OK\n");
          strcat(statuscode, "Content-Type: ");
          strcat(statuscode, fileinfo);
          strcat(statuscode, "\n");
          strcat(statuscode, "Content-Length ");
          strcat(statuscode, fileSizeStr);
          strcat(statuscode, "\n");
          strcat(statuscode, keepAliveStr);
          printf("%s\n", statuscode);
          i = write(connfd, statuscode, sizeofMessage);
          printf("%d bytes written\n", i);
          i = sendfile(connfd, currentfd, 0, fileSize);
          close(currentfd);
          printf("%d bytes written\n", i);
        }
        else{
          sizeofMessage = 1 + strlen(HTTPType) + strlen(" 404 Not Found: ") + strlen(requestedURI) + strlen("\n");
          char * errorcode = (char*) malloc(sizeofMessage);
          strcpy(errorcode, HTTPType);
          strcat(errorcode, " 404 Not Found: ");
          strcat(errorcode, requestedURI);
          strcat(errorcode, "\n");
          write(connfd, errorcode, sizeofMessage);
        }
      }
    }
    else{
      counter = 0;
      while(counter < strlen(defaultPages) && !correctDocument){
        char * defaultPage = (char *) malloc(1 + strlen(defaultPages));
        counter = parse(counter, defaultPages, defaultPage);
        char * defaultURI = (char *) malloc(1 + strlen(requestedURI) + strlen(defaultPage));
        strcpy(defaultURI, strtok(requestedURI, "\t\n"));
        strcat(defaultURI, defaultPage);
        char * requestedDocument = (char *) malloc(1 + strlen(root)+ strlen(defaultURI));
        strcpy(requestedDocument, root);
        strcat(requestedDocument, defaultURI);
        currentfd = open(requestedDocument, O_RDONLY);
        if (currentfd > 0){
          close(currentfd);
          correctDocument = 1;
          transmitMessage(connfd, root, contentTypes, defaultURI, HTTPType, keepAlive);
        }
        free(defaultPage);
        free(defaultURI);
        free(requestedDocument);
      }
      if(!correctDocument){
        counter = 0;
        char * defaultPage = (char *) malloc(1 + strlen(defaultPages));
        counter = parse(counter, defaultPages, defaultPage);
        char * defaultURI = (char *) malloc(1 + strlen(requestedURI) + strlen(defaultPage));
        strcpy(defaultURI, strtok(requestedURI, "\t\n"));
        strcat(defaultURI, defaultPage);
        transmitMessage(connfd, root, contentTypes, defaultURI, HTTPType, keepAlive);
        free(defaultPage);
        free(defaultURI);
      }
    }
  }
  else{
    sizeofMessage = 1 + strlen("HTTP/1.1 500	Internal	Server	Error:	cannot	allocate	memory\n");
    char * errorcode = (char*) malloc(sizeofMessage);
    strcpy(errorcode, "HTTP/1.1 500	Internal	Server	Error:	cannot	allocate	memory\n");
    write(connfd, errorcode, sizeofMessage);
  }
}

/*
  Starting from the current counter
  Parse the buffer to the next value surrounded by spaces and store htat in result
  return the updated value of counter
*/
int parse(int counter, char * buf, char * result)
{
  int i = 0;
  while (isspace(buf[counter]) && counter < MAXLINE){
    counter++;
  }
  while (!isspace(buf[counter]) && counter < MAXLINE){
    result[i] = buf[counter];
    counter++;
    i++;
  }
  //result[i] = "\0";
  result[i+1] = 0;
  return counter;
}

/*
  Starting from the current counter
  Parse the buffer till the next newline and store the value in result
  return the updated value of counter
*/
int parseTilNewline(int counter, char * buf, char * result){
  int i = 0;
  while (isspace(buf[counter]) && counter < MAXLINE){
    counter++;
  }
  while (buf[counter] != '\n' && counter < MAXLINE){
    result[i] = buf[counter];
    counter++;
    i++;
  }
  //result[i] = "\0";
  result[i+1] = 0;
  return counter;
}

/*
  Read the config file and store the values in the global variables for
    ContentTypes
    Port number
    Root Folder
    Default Pages
  Return the value if all of these are found
*/
int readConfig(int configfd){
  int counter = 0;
  char buf[MAXLINE];
  char firstWord[MAXLINE];
  int foundPort = 0, foundRoot = 0, foundContent = 0, foundDefault = 0, n;

  while((n = readline(configfd, buf, MAXLINE)) != 0) {
    char firstWord[MAXLINE];
    printf("%d", counter);
    counter = 0;
    counter = parse(counter, buf, firstWord);
    if (firstWord[0] != '#'){
      if (strstr(firstWord,"Listen") != NULL){
        foundPort = 1;
        counter = parse(counter, buf, portNum);
      }
      else if (strstr(firstWord,"DirectoryIndex") != NULL){
        foundDefault = 1;
        counter = parseTilNewline(counter, buf, defaultPages);
      }
      else if (strstr(firstWord,"DocumentRoot") != NULL){
        foundRoot = 1;
        counter = parse(counter, buf, root);
      }
      else if (strstr(firstWord, "ContentTypes") != NULL){
        counter = parseTilNewline(counter, buf, contentTypes);
        foundContent = 1;
      }
    }
  }
  if(close(configfd) < 0){
    return 0;
  }
  int newVal = foundPort & foundRoot & foundContent & foundDefault;
  return newVal;
}
