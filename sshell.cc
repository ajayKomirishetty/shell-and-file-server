#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include "tcp-utils.h"
#include "tokenize.h"
#include <errno.h>
#include <string>
#include <string.h>
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/syscall.h>

typedef int socket_type;

extern char *optarg;
extern int optind;
int  fport = 9001,sport = 9002; // sport - shell server port, fport - file server port
using namespace std;
int suppress_output = 1, delay_io = 0;

const char *logFile = "serverlog.log";        // log file contains all logs of the server
const char *errLogFile = "errorlog";          // log file contains all erroneous data
const char *path[] = {"/bin", "/usr/bin", 0}; // path, null terminated (static)
char recentArgv[256];
char *recentMessage;
char **envp;

pthread_mutex_t lockSeek, readWriteLock;

struct fileLock
{
  int file_id;
  string filename = "";
  int readers = 0;
  int writers = 0;
};
struct fileLock currentFiles[120]; // array of structure holds all the open files
int openfiles = 0;                 // count of files that are opened

const char *intToStr(int n)
{
  string s = to_string(n) + "\n";
  char const *temp = s.c_str(); //use char const* as target type
  return temp;
}

const char *toStr(int n)
{
  string s = to_string(n);
  char const *temp = s.c_str(); //use char const* as target type
  return temp;
}

int recentArgs(char **data)
{
  //	recentArgv = (char[])data;
}

/*void delay_read()
{
  if (delay_io == 1)
    sleep(0.5);
}

void delay_write()
{
  if (delay_io == 1)
    sleep(0.5);
}*/

int checkIfFileOpened(string filename)
{
  printf("reached check if file opened\n");
  for (int i = 0; i < openfiles; i++)
  {
    printf("filename is %s, \t current file is %s\t open fils is %d\n", filename, currentFiles[i].filename, openfiles);
    printf("fp of current file is %d\n", currentFiles[i].file_id);
    if (currentFiles[i].filename == filename)
    {
      return i;
    }
  }
  return -1;
}

void printOpenFiles()
{
  printf(".................open files............... %d\n", openfiles);
  for (int i = 0; i < openfiles; i++)
  {
    //	printf("current file is %s\n",currentFiles[i].filename);
    printf("fp of current file is %d\n", currentFiles[i].file_id);
  }
  printf(".................open files............... %d\n", openfiles);
}

/*
 * Cleans up "zombie children," i.e., processes that did not really
 * die.  Will run each time the parent receives a SIGCHLD signal.
 */
void cleanup_zombies(int sig)
{
  int status;
  printf("Cleaning up... \n");
  while (waitpid(-1, &status, WNOHANG) >= 0)
    /* NOP */;
}

void send_error(int sd, char *data)
{
  send(sd, "-1 ", strlen("-1 "), 0);
  send(sd, data, strlen(data), 0);
  send(sd, "\n", 1, 0);
}

/*
 * Dummy reaper, set as signal handler in those cases when we want
 * really to wait for children.  Used by run_it().
 *
 * Note: we do need a function (that does nothing) for all of this, we
 * cannot just use SIG_IGN since this is not guaranteed to work
 * according to the POSIX standard on the matter.
 */
void block_zombie_reaper(int signal)
{
  /* NOP */
}

int createLogFile()
{
  int confd = open(logFile, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
  if (confd > 0)
  {
    if (suppress_output == 0)
    {
      printf("log file created successfully\n");
    }
    else
    {
      dup2(confd, 1); // make stdout go to file
      dup2(confd, 2); // make stderr go to file
    }
    close(confd);
    return 0;
  }
  else
  {
    printf("unable to create log file, error: %d \n", errno);
    return -1;
  }
}

int open_file(int sd, char *filename)
{
  printOpenFiles();
  int opened = checkIfFileOpened(filename);
  printf("file opened value is %d\n", opened);
  if (opened >= 0) // file is already opened by some client
  {
    printf("file is already opened by another client\n");
    send(sd, "ERR ", strlen("ERR "), 0);
    int fd1 = currentFiles[opened].file_id;
    char buffer1[50];
    sprintf(buffer1, "%d", fd1);
    send(sd, buffer1, strlen(buffer1), 0);
    send(sd, " File is already opened by another client\n", sizeof(" File is already opened by another client\n"), 0);
    return fd1;
  }
  else if (opened == -1)
  { // file is not opened by any other clients
    int fp = open(filename, O_RDWR);
    if (fp > 0)
    {
      printf("file opened successfully\n");
      send(sd, "OK ", strlen("OK "), 0);

      char buffer[50];
      sprintf(buffer, "%d success", fp);

      send(sd, buffer, strlen(buffer), 0);
      send(sd, "\n", 1, 0);
      printf("storing this file at position %d\n", openfiles);
      currentFiles[openfiles].file_id = fp;
      printf("storing filename as %s in file pointed %d\n", filename, fp);
      currentFiles[openfiles].filename = filename;
      openfiles = openfiles + 1;
      //storeOpenFiles(filename,fp);
      return fp;
    }
    else
    {
      printf("unable to open file : %d\n", fp);
      send(sd, "FAIL ", strlen("FAIL "), 0);
      send(sd, "-1 ", strlen("-1 "), 0);
      send(sd, "unable to open file", strlen("unable to open file"), 0);
      send(sd, "\n", 1, 0);
      return -1;
    }
  }
}

int seek_file(int sd, int fp, int offset)
{
  if (sd > 0)
  {
    pthread_mutex_lock(&lockSeek);
    int len = lseek(fp, offset, SEEK_CUR);
    printf("error is %s\n", strerror(errno));
    send(sd, "OK ", strlen("OK "), 0);
    char buffer[50];
    sprintf(buffer, "%d success\n", len);
    send(sd, buffer, sizeof(buffer), 0);
    pthread_mutex_unlock(&lockSeek);
    return len;
  }
  else
  {
    send(sd, "ERR ", strlen("ERR "), 0);
    send(sd, strerror(errno), strlen(strerror(errno)), 0);
    return -1;
  }
}

int read_file(int sd, int fp, int length)
{
  
  if(delay_io == 1){
	send(sd,"Delaying Read operation:",strlen("Delaying Read operation:"),0);
	for( int i=1; i<=6; i++){
		usleep(490000);
		send(sd,".",1,0);
		send(sd,"\n",1,0);
	}
	send(sd,"Delayed by 3 seconds",strlen("Delayed by 3 seconds"),0);
	send(sd,"\n",1,0);
  }

  char *buf;

  if (fp > 0)
  {
    buf = (char *)malloc(sizeof(char) * length);
    if (buf == NULL)
    {
      fputs("Memory error", stderr);
      exit(2);
    }
    printf("retreiving data from fd: %d\n", fp);
    // copy the file into the buffer:

    int result = read(fp, buf, length);
    buf[(length + 1)] = '\0';
    printf("%d bytes read from file\n", result);
    if (result == 0)
    {
      send(sd, "ERR ", strlen("ERR "), 0);
      send_error(sd, " 0 bytes read from the file");
      printf("Reading error: %s\n", strerror(errno));
      return 0;
    }
    char buffer1[50];
    sprintf(buffer1, "OK %d", result);
    send(sd, buffer1, strlen(buffer1), 0);
    send(sd, " ", strlen(" "), 0);
    send(sd, buf, result, 0);
    send(sd, "\nOK 0 success", strlen("\nOK 0 success"), 0);
    send(sd, "\n", 1, 0);
    return fp;
  }
  else
  {
    printf("unable to open file : %d\n", fp);
    send(sd, "\n", 1, 0);
    return -1;
  }
}

int storeRecentCommand(char *const argv[])
{
  int confd = open("temp.txt", O_RDONLY | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
  char *temp;
  for (int i = 0; argv[i] != 0; i++)
  {
    write(confd, argv[i], strlen(argv[i]));
    write(confd, " ", strlen(" "));
  }
  close(confd);
}

int run_it(const char *command, char *const argv[], const char **path, int sd)
{

  // we really want to wait for children so we inhibit the normal
  // handling of SIGCHLD
  signal(SIGCHLD, block_zombie_reaper);

  int childp = fork();
  int status = 0;

  if (childp == 0)
  { // child does execve
#ifdef DEBUG
    fprintf(stderr, "%s: attempting to run (%s)", __FILE__, command);
    for (int i = 1; argv[i] != 0; i++)
      fprintf(stderr, " (%s)", argv[i]);
    fprintf(stderr, "\n");
#endif
    execve(command, argv, envp); // attempt to execute with no path prefix...
    for (size_t i = 0; path[i] != 0; i++)
    { // ... then try the path prefixes
      char *cp = new char[strlen(path[i]) + strlen(command) + 2];
      sprintf(cp, "%s/%s", path[i], command);
#ifdef DEBUG
      fprintf(stderr, "%s: attempting to run (%s)", __FILE__, cp);
      for (int i = 1; argv[i] != 0; i++)
        fprintf(stderr, " (%s)", argv[i]);
      fprintf(stderr, "\n");
#endif
      execve(cp, argv, envp);
      delete[] cp;
    }

    // If we get here then all execve calls failed and errno is set
    char *message = new char[strlen(command) + 10];
    sprintf(message, "exec %s", command);
    //perror(message);
    dup2(sd, 2);
    send(sd, "ERR -1 ", strlen("ERR -1 "), 0);
    perror("ERR -1 ");
    perror(message);
    //send(sd,command,strlen(command),0);
    //send_error(sd,message);
    delete[] message;
    exit(errno); // crucial to exit so that the function does not
                 // return twice!
  }

  else
  { // parent just waits for child completion
    waitpid(childp, &status, 0);
    // we restore the signal handler now that our baby answered
    signal(SIGCHLD, cleanup_zombies);
    return status;
  }
}

int recentOutput(int sd, int available)
{
  if (available == 1)
  {
    int confd = open("temp.txt", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
    char command[129]; // buffer for commands
    command[128] = '\0';
    char *com_tok[129]; // buffer for the tokenized commands
    size_t num_tok;     // number of tokens
    readline(confd, command, 128);
    num_tok = str_tokenize(command, com_tok, strlen(command));
    com_tok[num_tok] = 0;
    int saved_stdout = dup(1);
    int saved_stderr = dup(2);
    dup2(sd, 1);
    dup2(sd, 2);
    int r = run_it(com_tok[0], com_tok, path, sd);
     if (r > 0)
        printf("OK 0 success\n");
    dup2(saved_stdout, 1);
    close(saved_stdout);
    dup2(saved_stderr, 2);
    close(saved_stderr);
    close(confd);
  }
  else
  {
    send(sd, "ERR ", strlen("ERR "), 0);
    char buffer[50];
    sprintf(buffer, "%d", EIO);
    send(sd, buffer, strlen(buffer), 0);
    send(sd, " input/output error\n", strlen(" input/output error\n"), 0);
  }
}

char *recent_command()
{
  return recentMessage;
}

void do_shell_server(int sd)
{
  printf("Incoming client.\n");

  const int ALEN = 256;
  char req[ALEN];
  int n;


  char *com_tok[129], *prev_com_tok[129]; // buffer for the tokenized commands
  char *temp[129];
  size_t num_tok; // number of tokens

  int available = 0;
  // Loop while the client has something to say...
  while ((n = readline(sd, req, ALEN - 1)) != recv_nodata)
  {
    num_tok = str_tokenize(req, com_tok, strlen(req));
    com_tok[num_tok] = 0;

    if (strcmp(com_tok[0], "CPRINT") == 0)
    {
      recentOutput(sd, available);
    }
    else
    {
      available = 1;
      storeRecentCommand(com_tok);

      int r = run_it(com_tok[0], com_tok, path, sd);
      if (r > 0)
      {
        //send(sd,"OK 0",strlen("OK 0"),0);
        //send(sd,strerror(errno),strlen(strerror(errno)),0);
        //send(sd,"\n",1,0);
      }
      else
      {
        printf("%s completed with a non-null exit code (%d)\n", com_tok[0], WEXITSTATUS(r));
        send(sd, "OK 0 success\n", strlen("OK 0 success\n"), 0);
      }
    }
  }
  // read 0 bytes = EOF:
  printf("Connection closed by client.\n");
  shutdown(sd, 1);
  close(sd);
  printf("Outgoing client removed.\n");
}

/*
 * Repeatedly receives requests from the client and responds to them.
 * If the received request is an end of file or "quit", terminates the
 * connection.  Note that an empty request also terminates the
 * connection.  Same as for the purely iterative server.
 */

void print_enoent(int sd)
{
  send(sd, "ERR ", strlen("ERR "), 0);
  char buffer[50];
  sprintf(buffer, "%d", ENOENT);
  send(sd, buffer, strlen(buffer), 0);
  send(sd, " no such file or directory\n", strlen(" no such file or directory\n"), 0);
}

void do_client(int sd)
{
  printf("Incoming client.\n");

  const int ALEN = 256;
  char req[ALEN];
  int n;

  char *com_tok[129]; // buffer for the tokenized commands
  size_t num_tok;     // number of tokens

  // Loop while the client has something to say...
  while ((n = readline(sd, req, ALEN - 1)) != recv_nodata)
  {

    if (strcmp(req, "quit") == 0)
    {
      printf("Received quit, sending EOF.\n");
      shutdown(sd, 1);
      return;
    }

    num_tok = str_tokenize(req, com_tok, strlen(req));
    com_tok[num_tok] = 0;

    if (num_tok == 1 && (strcmp(com_tok[0], "FOPEN") == 0 || strcmp(com_tok[0], "FCLOSE") == 0))
    {
      printf("%s: too few arguments\n", com_tok[0]);
      send(sd, "ERR ", strlen("ERR "), 0);
      send_error(sd, "too few arguments");
      continue;
    }

    if (num_tok <= 2 && (strcmp(com_tok[0], "FSEEK") == 0 || strcmp(com_tok[0], "FREAD") == 0 || strcmp(com_tok[0], "FWRITE") == 0))
    {
      printf("%s: too few arguments\n", com_tok[0]);
      send(sd, "ERR ", strlen("ERR "), 0);
      send_error(sd, "too few arguments");
      continue;
    }

    if (strlen(req) == 0) // no command, luser just pressed return
      printf("no data received from client");

    if (strcmp(com_tok[0], "FOPEN") == 0)
    {
      if (strlen(com_tok[1]) == 0)
      {
        printf("FOPEN: too few arguments\n");
        continue;
      }
      int fp = open_file(sd, com_tok[1]); // open the file requested by client
      printOpenFiles();

      if (fp > 0)
      {
      }
      else
      {
        // take action if file is not opened
      }
    }
    else if (strcmp(com_tok[0], "FREAD") == 0)
    {
      if (strlen(com_tok[2]) == 0)
      {
        printf("FOPEN: too few arguments\n");
        send(sd, "ERR ", strlen("ERR "), 0);
        send_error(sd, "FOPEN: too few arguments");
        continue;
      }
      struct fileLock openedFile;
      int found = 0;
      if (openfiles > 0)
      {
        for (int i = 0; i < openfiles; i++)
        {
          if (currentFiles[i].file_id == atol(com_tok[1]))
          {
            openedFile = currentFiles[i];
            found = 1;
          }
        }
      }
      if (found == 1)
      {
        pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
        // declaring mutex
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        if (openedFile.writers == 0)
        {
          printf("number of readers are %d\n", openedFile.readers);
          pthread_mutex_lock(&readWriteLock);
          openedFile.readers = openedFile.readers + 1;
          printf("number of readers are %d\n", openedFile.readers);
          read_file(sd, atol(com_tok[1]), atol(com_tok[2])); // open the file requested by client
          //send(sd,"-1",strlen("-1"),0);
          //send(sd,"\n",strlen("\n"),0);
          //}
          openedFile.readers = openedFile.readers - 1;
          pthread_mutex_unlock(&readWriteLock);
          //pthread_cond_signal(&cond1);
        }
        else
        {
          pthread_cond_wait(&cond1, &lock);
        }
      }
      else
      {
        print_enoent(sd);
      }
    }

    else if (strcmp(com_tok[0], "CPRINT") == 0)
    {
      char *test = recent_command();
      send(sd, test, strlen(test), 0);
      send(sd, "\n", 1, 0);
    }

    else if (strcmp(com_tok[0], "FCLOSE") == 0)
    {
      if (strlen(com_tok[1]) == 0)
      {
        printf("FCLOSE: too few arguments\n");
        //send_error(sd);
        continue;
      }
      //checkIfFileOpened(filename);
      int found = 0, index = 0;
      struct fileLock openedFile;
      printf("opned files is %d\n", openfiles);
      if (openfiles > 0)
      {
        for (int i = 0; i < openfiles; i++)
        {
          printf("fp is %d\tcurrent file id is %d\n", atol(com_tok[1]), currentFiles[i].file_id);
          if (currentFiles[i].file_id == atol(com_tok[1]))
          {
            openedFile = currentFiles[i];
            openedFile.file_id = 0;
            openedFile.filename = "";
            openedFile.readers = 0;
            openedFile.writers = 0;
            found = 1;
            index = i;
          }
        }
      }
      if (found == 1)
      {
        printf("I have found\n index is %d\n", index);

        //removeFromSavedFile(atol(com_tok[1]));
        for (int i = index; i < openfiles; i++)
        {
          currentFiles[i] = currentFiles[i + 1];
        }
        printf("deleted from struct successfully\n");
        printf("no of open files before close is %d\n", openfiles);
        int a = close(atol(com_tok[1]));
        printf("closed fd : %d\t and return from close is %d\n", atol(com_tok[1]), a);
        openfiles = openfiles - 1;
        printf("no of open files after close is %d\n", openfiles);
        send(sd, "OK ", strlen("OK "), 0);
        char buffer1[50];
        sprintf(buffer1, "%d success\n", a);
        send(sd, buffer1, strlen(buffer1), 0);
      }
      else
      {
        print_enoent(sd);
      }
    }

    else if (strcmp(com_tok[0], "FWRITE") == 0)
    {
    if(delay_io == 1){
	send(sd,"Delaying Write operation:",strlen("Delaying Write operation:"),0);
	for( int i=1; i<=12; i++){
		usleep(490000);
		send(sd,".",1,0);
		send(sd,"\n",1,0);
	}
	send(sd,"Delayed by 6 seconds",strlen("Delayed by 6 seconds"),0);
	send(sd,"\n",1,0);
     }

      int n;
      if (strlen(com_tok[2]) == 0)
      {
        printf("FCLOSE: too few arguments\n");
        send_error(sd, "FWRITE: too few arguments");
        continue;
      }
      struct fileLock openedFile;
      int found = 0;
      if (openfiles > 0)
      {
        for (int i = 0; i < openfiles; i++)
        {
          if (currentFiles[i].file_id == atol(com_tok[1]))
          {
            openedFile = currentFiles[i];
            found = 1;
          }
        }
      }
      if (found == 1)
      {

        pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
        // declaring mutex
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

        if (openedFile.writers == 0 && openedFile.readers == 0)
        {
          //printf("%d is number of writers\n",currentFile.writers);
          openedFile.writers = openedFile.writers + 1;
          pthread_mutex_lock(&readWriteLock);
          for (size_t i = 1; com_tok[i] != 0; i++)
          {
            n = write(atol(com_tok[1]), com_tok[i], strlen(com_tok[i]));
            write(atol(com_tok[1]), " ", 1);
            printf("%d bytes written into file\n", n);
            printf("%d is file descriptor\n", atol(com_tok[1]));
            if (errno > 0)
              printf("%s\n", strerror(errno));
          }
          printf("%d is number of writers\n", openedFile.writers);
          openedFile.writers = openedFile.writers - 1;
          //pthread_cond_signal(&cond1);
        }
        else
        {
          pthread_cond_wait(&cond1, &lock);
        }
        //int a = write(sd,com_tok[2],strlen(com_tok[2]));
        send(sd, "OK 0 success\n", strlen("OK 0 success\n"), 0);
        pthread_mutex_unlock(&readWriteLock);
      }
      else
      {
        print_enoent(sd);
      }
    }

    else if (strcmp(com_tok[0], "FSEEK") == 0)
    {
      if (strlen(com_tok[2]) == 0)
      {
        printf("FSEEK: too few arguments\n");
        send_error(sd, "FSEEK: too few arguments");
        continue;
      }
      struct fileLock openedFile;
      int found = 0;
      if (openfiles > 0)
      {
        for (int i = 0; i < openfiles; i++)
        {
          if (currentFiles[i].file_id == atol(com_tok[1]))
          {
            openedFile = currentFiles[i];
            found = 1;
          }
        }
      }
      if (found == 1)
      {
        pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
        // declaring mutex
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        if (openedFile.writers == 0 && openedFile.readers == 0)
        {
          printf("%d is number of writers\n", openedFile.writers);
          openedFile.writers = openedFile.writers + 1;
          pthread_mutex_lock(&readWriteLock);

          if (seek_file(sd, atol(com_tok[1]), atol(com_tok[2])) < 0)
          { // open the file requested by client
            // take action if file is not opened
            send(sd, "ERR ", strlen("ERR "), 0);
            send(sd, strerror(errno), strlen(strerror(errno)), 0);
            send(sd, " File is not opened\n", strlen(" File is not opened\n"), 0);
          }
          pthread_mutex_unlock(&readWriteLock);
          printf("%d is number of writers\n", openedFile.writers);
          openedFile.writers = openedFile.writers - 1;
          //pthread_cond_signal(&cond1);
        }
        else
        {
          pthread_cond_wait(&cond1, &lock);
        }
      }
      else
      {
        print_enoent(sd);
      }
    }

    else
    {
      send(sd, "ERR ", strlen("ERR "), 0);
      send(sd, "-1 ", strlen("-1 "), 0);
      send(sd, "command not found", strlen("command not found"), 0);
      send(sd, "\n", 1, 0);
      continue;
    }
  }

  printf("Connection closed by client.\n");
  shutdown(sd, 1);
  close(sd);
  printf("Outgoing client removed.\n");
}

void process_switches(int argc, char **argv)
{
  int c;
  while ((c = getopt(argc, argv, "f:s:dD")) != -1)
  {
    switch (c)
    {
    case 'f':
    {
      //printf("Option found: f, optind = %s\n",optarg);
      fport = atoi(optarg);
      //printf("fport = %d\n",fport);
      break;
    }

    case 's':
    {
      //printf("Option found: s, optind = %s\n",optarg);
      sport = atoi(optarg);
      break;
    }
    case 'd':
    {
      //printf("Option found: d, optind = %s\n",optarg);
      suppress_output = 0;
      break;
    }
    case 'D':
    {
      delay_io = 1;
      //printf("Option found: D, optind = %s\n",optarg);
      break;
    }
    case 'v':
    {
      printf("Option found: v, optind = %s\n", optarg);
      break;
    }
    case '?':
      printf("Didn't you enter an invalid option?\n");
      break;
    default:
      // usage();
      break;
    }
  }
}

socket_type network_accept_any(socket_type fds[], unsigned int count, struct sockaddr *addr, socklen_t *addrlen)
{
  fd_set readfds;
  socket_type maxfd, fd;
  unsigned int i;
  int status;

  FD_ZERO(&readfds);
  maxfd = (socket_type)-1;
  for (i = 0; i < count; i++)
  {
    FD_SET(fds[i], &readfds);
    if (fds[i] > maxfd)
      maxfd = fds[i];
  }
  status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
  if (status < 0)
    return -1;
  fd = (socket_type)-1;
  for (i = 0; i < count; i++)
    if (FD_ISSET(fds[i], &readfds))
    {
      fd = fds[i];
      break;
    }
  if (fd == -1)
    return -1;
  else
    //return accept(fd, addr, addrlen);
    return fd;
}

int main(int argc, char **argv, char **envp1)
{
  envp = envp1;
  const int qlen = 32;                                // incoming connections queue length
  int msock, ssock, msock1;                           // master and slave socket
  struct sockaddr_in client_addr;                     // the address of the client...
  unsigned int client_addr_len = sizeof(client_addr); // ... and its length

  process_switches(argc, argv); // process all the arguments

  msock = passivesocket(fport, qlen);  // create file server
  msock1 = passivesocket(sport, qlen); // create shell server

  if (msock < 0)
  {
    perror("passivesocket");
    return 1;
  }

  if (msock1 < 0)
  {
    perror("passivesocket");
    return 1;
  }

  printf("File Server up and listening on port %d.\n", fport);
  printf("Shell Server up and listening on port %d.\n", sport);

  int fds[2];
  fds[0] = msock;
  fds[1] = msock1;
  int ms = 0;
  pthread_t tt, tt1;
  pthread_attr_t ta, ta1;
  pthread_attr_init(&ta);
  pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED);
  pthread_attr_init(&ta1);
  pthread_attr_setdetachstate(&ta1, PTHREAD_CREATE_DETACHED);

  int fd = open("/dev/tty", O_RDWR);
  ioctl(fd, TIOCNOTTY, 0);
  close(fd);

  
  signal(SIGHUP,SIG_IGN);
 //signal(SIGINT,SIG_IGN);
 //signal(SIGTERM,SIG_IGN);

  createLogFile(); // create a log file to record server logs

  signal(SIGCHLD, cleanup_zombies); // clean up errant children...

  while (1)
  {
    // Accept connection:

    ms = network_accept_any(fds, 2, (struct sockaddr *)&client_addr, &client_addr_len);
    if (ms == -1)
      continue;
    ssock = accept((long int)ms, (struct sockaddr *)&client_addr, &client_addr_len);

    if (ssock < 0)
    {
      if (errno == EINTR)
        continue;
      perror("accept");
      return 1;
    }

    if (ms == msock1)
    {
      if (pthread_create(&tt, &ta, (void *(*)(void *))do_shell_server, (void *)ssock) != 0)
      {
        perror("pthread_create");
        return 1;
      }
    }
    else if (ms == msock)
    {
      if (pthread_create(&tt1, &ta1, (void *(*)(void *))do_client, (void *)ssock) != 0)
      {
        perror("pthread_create");
        return 1;
      }
    }
  }
  return 0; // will never reach this anyway...
}