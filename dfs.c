/*
 * echoserver.c - A sequential echo server
 */

#include "nethelp.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>

typedef struct {
  char* username;
  char* password;
  struct userPass* next;
} userPass;

//Default values read from config file are stored as globals so they can be utilized by all threads
char root[100], contentTypes[100], defaultPages[100], portNum[10];
char * directory;
void *thread(void *vargp);
int readConfig(int configfd);
void parseRequest(int connfd, struct userPass * firstUser);
void transmitMessage(int connfd, char* root, char* contentTypes, char* requestedURI, char* HTTPType, int keepAlive);
int parse(int counter, char * buf, char * result);
int parseTilNewline(int counter, char * buf, char * result);

int main(int argc, char **argv)
{
    int listenfd, connfd, port, clientlen=sizeof(struct sockaddr_in);
    int configFD;
    char* filename;
    int * connfdp;
    char * newDir;
    struct sockaddr_in clientaddr;
    if (argc != 4) {
	     fprintf(stderr, "usage: %s <conf File Location> <Directory> <Port Number>\n", argv[0]);
       exit(0);
    }
    filename = argv[1];
    directory = argv[2];
    strcpy(root,"/root/NetworkSystems/DFS/");
    newDir = (char *) malloc( 1 + strlen(root) + strlen(directory));
    strcpy(newDir, root);
    strcat(newDir, directory);
    mkdir(newDir, 0777);
    port = atoi(argv[3]);
    printf("%s", filename);
    configFD = open(filename, O_RDONLY);
    if (configFD < 0){
      fprintf(stderr, "Bad filename\n");
      exit(0);
    }
    userPass *firstUser;
    fprintf(stderr, "ReadingConfig\n");
    firstUser = readConfig(configFD);
    /*
    while (firstUser != NULL){
      printf("Username: %s Password: %s\n", firstUser->username, firstUser->password);
      firstUser = firstUser->next;
    }
    */
    listenfd = open_listenfd(port);
    while (1) {
      //connfdp = malloc(sizeof(int));
      connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
      parseRequest(connfdp, firstUser);
      close(connfdp);
    }
}


/*Parses the incoming request form the socket
  If a GET request is found this function calls transmitMessage()
  To write the headers and file to the server
  If told to keep connection alive it is impolemented with a sleep timer
  and a counter that is incremented till 10
*/
void parseRequest(int connfd, struct userPass * firstUser)
{
    int n, fileSize;
    char buf[MAXLINE];
    char * username;
    char * password;
    char * requestedDoc;
    char * fileName;
    char * writeDoc;
    DIR *d;
    struct dirent *dir;
    struct stat st;
    int counter = 0, i, sizeofMessage;
    int timer = 10, keepAlive = 0, authenticated = 0;
    int get = 0, list = 0, put = 0;
    int readingDocument = 0;
    int putFD, currentfd;
    //int flags = fcntl(connfd, F_GETFL, 0);
    //fcntl(connfd, F_SETFL, flags | O_NONBLOCK);
    do{
      fprintf(stderr, "Timer");
      while((n = readline(connfd, buf, MAXLINE)) > 0) {
        timer = 10;
        fprintf(stderr, "server received %d bytes\n", n);
        printf("buf reads %s", buf);
        list = 0;
        get = 0;
        counter = 0;
        char* firstWord = malloc(1 + strlen(buf));
        counter = parse(counter, buf, firstWord);
        if (strstr(firstWord, "GET") != NULL){
          requestedDoc = malloc(1 + strlen(buf));
          username = malloc(1 + strlen(buf));
          password = malloc(1 + strlen(buf));
          counter = parse(counter, buf, requestedDoc);
          counter = parse(counter, buf, username);
          counter = parse(counter, buf, password);
          get = 1;
        }
        else if (strstr(firstWord, "LIST") != NULL){
          username = malloc(1 + strlen(buf));
          password = malloc(1 + strlen(buf));
          counter = parse(counter, buf, username);
          counter = parse(counter, buf, password);
          list = 1;
        }
        if (list || get){
          if(put){
            close(putFD);
          }
          put = 0;
          userPass * currentUser = malloc(sizeof(userPass));
          currentUser = firstUser;
          //currentUser->username = firstUser->username;
          //currentUser->password = firstUser->password;
          //currentUser->next = firstUser->next;
          while (currentUser != NULL){
            if(username == currentUser->username){
              if(password == currentUser->password){
                authenticated = 1;
              }
            }
            currentUser = currentUser->next;
          }
          free(currentUser);
          fprintf(stderr, "Authenticating");
          if(!authenticated){
            sizeofMessage = 1 + strlen("Invalid	Username/Password.	Please	try	again.\n");
            write(connfd, "Invalid	Username/Password.	Please	try	again.\n", sizeofMessage);
          }
          else{
            char* dirname = (char *) malloc(1 + strlen(root) + strlen(directory) + strlen("/")+ strlen(username) + strlen("/"));
            strcpy(dirname, root);
            strcat(dirname, directory);
            strcat(dirname, "/");
            strcat(dirname, username);
            strcat(dirname, "/");
            mkdir(dirname, 0777);
            if(get){
              char * requestedFilePath = (char *) malloc(1 + strlen(dirname) + strlen(requestedDoc));
              strcpy(requestedFilePath, dirname);
              strcat(requestedFilePath, requestedDoc);
              currentfd = open(requestedFilePath, O_RDONLY);
              if(currentfd > 0){
                stat(requestedFilePath, &st);
                fileSize = st.st_size;
                char fileSizeStr[15];
                sprintf(fileSizeStr, "%d", fileSize);
                i = sendfile(connfd, currentfd, 0, fileSize);
                close(currentfd);
              }
              else{
                sizeofMessage = 1 + strlen("Error: File Not Found\n");
                i = write(connfd, "Error: File Not Found\n", sizeofMessage);
              }
            }
            else{
              d = opendir(dirname);
              if (d){
                while ((dir = readdir(d)) != NULL)
                {
                    sizeofMessage = 1 + strlen(dir->d_name) + strlen("\n");
                    char * fileNames = (char *) malloc(sizeofMessage);
                    strcpy(fileNames, dir->d_name);
                    strcat(fileNames, "\n");
                    i = write(connfd, fileNames, sizeofMessage);
                    free(fileNames);
                }
                closedir(d);
              }
            }
          }
          authenticated = 0;
        }
        if (strstr(firstWord, "PUT") != NULL){
          writeDoc = malloc(1 + strlen(buf));
          username = malloc(1 + strlen(buf));
          password = malloc(1 + strlen(buf));
          counter = parse(counter, buf, writeDoc);
          counter = parse(counter, buf, username);
          counter = parse(counter, buf, password);
          userPass * currentUser = malloc(sizeof(userPass));
          currentUser = firstUser;
          while (currentUser != NULL){
            if(username == currentUser->username){
              if(password == currentUser->password){
                authenticated = 1;
              }
            }
            currentUser = currentUser->next;
          }
          free(currentUser);
          if(!authenticated){
            sizeofMessage = 1 + strlen("Invalid	Username/Password.	Please	try	again.\n");
            write(connfd, "Invalid	Username/Password.	Please	try	again.\n", sizeofMessage);
          }
          else{
            char* dirname = (char *) malloc(1 + strlen(root) + strlen(directory) + strlen("/")+ strlen(username) + strlen("/"));
            strcpy(dirname, root);
            strcat(dirname, directory);
            strcat(dirname, "/");
            strcat(dirname, username);
            strcat(dirname, "/");
            mkdir(dirname, 0777);
            char * requestedWriteDoc = (char *) malloc(1 + strlen(dirname) + strlen(writeDoc));
            strcpy(requestedWriteDoc, dirname);
            strcat(requestedWriteDoc, writeDoc);
            putFD = open(requestedWriteDoc, O_WRONLY);
            if(putFD > 0){
              put = 1;
            }
          }
        }
        else if (put){
          write(putFD, buf, n);
        }
      }
      if(put){
        close(putFD);
      }
      sleep(1);
      timer--;
    }while((timer > 0));
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
  int n;
  int first = 0;
  userPass * firstUser = malloc(sizeof(userPass));
  userPass * currentUser = malloc(sizeof(userPass));
  while((n = readline(configfd, buf, MAXLINE)) != 0) {
    //char firstWord[MAXLINE];
    //counter = parse(counter, buf, firstWord);
    if (n > 1){
      userPass * newUser = malloc(sizeof(userPass));
      counter = 0;
      newUser->username = malloc(1+strlen(buf));
      newUser->password = malloc(1+strlen(buf));
      counter = parse(counter, buf, newUser->username);
      counter = parse(counter, buf, newUser->password);
      if (first == 0){
        first = 1;
        firstUser = newUser;
        currentUser = firstUser;
      }
      else{
        currentUser->next = newUser;
        currentUser = currentUser->next;
        currentUser->next = NULL;
      }
    }
  }
  if(close(configfd) < 0){
    return 0;
  }
  return firstUser;
}
