
Contents:

o  source code, included in sshell.cc
o  modules tokenize and tcp-utils as provided previously (note: the module tcp-utils is included because of the function    `readline')
o  a makefile (the default target builds the executable, there are no other targets of interest)
o  a small file used for testing of file operations from client side(test)
o  a log file (serverlog.log) which contains the detailed description of server logs. (this file is auto generated for the    first time and    it will be used by the server for logging purposes) 



Change the makefile and compile with -DDEBUG option for an executable
that prints debugging output to the standard error stream.


User guide and program description:

The server accepts two kind of clients (that request connections on two different ports), and runs 2 services namely shell server and file server.


There are few command line arguments for the executable. 
They are:

-s : o This argument takes the port number of shell server. On startup, server takes this argument and creates a socket at         this port numer.
     o When this argument is not supplied, shell server will run on 9002 port by default
 
-f : o  This argument takes the port number of file server. On startup, server takes this argument and creates a socket at          this port numer.
     o  When this argument is not supplied, shell server will run on 9001 port by default

-d : o detach mode.
     o This argument makes the server to log its status and its output,error to console (Terminal)
     o When this argument is not supplied, server logging will be done into a log file(serverlog.log)

-D : o When this argument is supplied, server runs in debug mode. it sleeps for read and write operations

 
Once launched, the executable behaves as follows:



o  The server on startup, creates a log file (if it is not available/ created before) 

o  The server is not interactive, any commands entered on server's terminal are ignored and no response will be sent back

o  Server runs two services, and listens on two ports. Whenever a new client connection arrives, server creates a posix    thread which is solely responsible for serving client requests

NOTE: All responses from file server and shell server will be of the following form

		status code message

where o  status : which is one of (OK/ERR/FAIL), used to determine if the command was a success or failure
      o  code   : check for respective code of each command in the below section
      o  message: readable description of code.

FILE server:

o  Below are few commands which are recognised by file server

FOPEN: o  This command takes in file name (can be relative path and absolute path) as argument and opens the file at server 
	  side 
	
	ARGUMENTS    : o filename (can be relative or absolute)

	RETURN VALUE : o  returns file descriptor of opened file as response to client if it is opened successfully.
		       o   returns -1 / Error code  and appropriate message if it encounters any error	
	
	TESTS       :  o 1) FOPEN test (test is the file name which is at root folder of submission)
			 2) FOPEN blah (blah an unkown file which doesnt exist anywhere)

	OUTPUT      :  o 1) OK 3 success (OK - command executes successfully, 3 - file descriptor, sucess - success)
			 2) ERR 2 no such file/directory
	 

FREAD o  This command takes in file descriptor as argument and attempts to read the file at server
	
	ARGUMENTS    :o file descriptor of previosly opened file

	RETURN VALUE: o  returns number of bytes read and actual bytes read if read operation is successful.
		      o  returns -1 / Error code  and appropriate message if it encounters any error	
		      o  Returns ERR -1 no such file/ directory if the specified file descriptor is not opened at server.

	TESTS       :  o 1) FREAD 3 10(3 is descriptor of file name "test" which is opened in above step)
			 2) FREAD 9 10(9 is random value which doent relate to any opened file descriptor)

	OUTPUT      :  o 1) OK 10 hello Ajay(OK - command executes successfully, 10 - number of bytes read, hello Ajay- 			            10 bytes of data from file)
			 2) ERR 2 no such file/directory	


FWRITE  o  This command takes in file descriptor as argument and attempts to write data in the file at server
	
	ARGUMENTS    :o file descriptor of previosly opened file

	RETURN VALUE: o  returns o on successful execution.
		      o  returns -1 / Error code  and appropriate message if it encounters any error	
		      o  Returns ERR -1 no such file/ directory if the specified file descriptor is not opened at server.

	TESTS       :  o 1) FWRITE 3 10(3 is descriptor of file name "test" which is opened in above step)
			 2) FWRITE 9 10(9 is random value which doent relate to any opened file descriptor)

	OUTPUT      :  o 1) OK 0 success (OK - command executes successfully, 0 - successfull, sucess - success)
			 2) ERR 2 no such file/directory


FSEEK  o  This command takes in file descriptor as argument and attempts to seek data in the file at server
	
	ARGUMENTS    :o file descriptor of previosly opened file

	RETURN VALUE: o  returns o on successful execution.
		      o  returns -1 / Error code and appropriate message if it encounters any error	
		      o  Returns ERR -1 no such file/ directory if the specified file descriptor is not opened at server.

	TESTS       :  o 1) FSEEK 3 10(3 is descriptor of file name "test" which is opened in above step)
			 2) FSEEK 9 10(9 is random value which doent relate to any opened file descriptor)

	OUTPUT      :  o 1) OK 10 success (OK - command executes successfully, 10 - advanced position of file pointer, 			    sucess - success)
			 2) ERR 2 no such file/directory

FCLOSE  o  This command takes in file descriptor as argument and attempts to close file at server
	
	ARGUMENTS    :o file descriptor of previosly opened file

	RETURN VALUE: o  returns o on successful execution.
		      o  returns -1 / Error code  and appropriate message if it encounters any error	
		      o  Returns ERR -1 no such file/ directory if the specified file descriptor is not opened at server.

	TESTS       :  o 1) FCLOSE 3(3 is descriptor of file name "test" which is opened in above step)
			 2) FCLOSE 9 (9 is random value which doent relate to any opened file descriptor)

	OUTPUT      :  o 1) OK 0 success (OK - command executes successfully, 0 - success, success - success)
			 2) ERR 2 no such file/directory
	
SHELL server:

	o This typical shell server takes in a command from client and then executes the command at server and doesnt send 	  back the actual response of the excuted command, but client can still request output of previous command by using
	  CPRINT. 

	TESTS       :  o 1) ls 
			 2) CPRINT (if ther is no command in current session it returns "ERR 5 input/output error")

	OUTPUT      :  o 1) OK 0 success
			 2) list of all files at server location


A typical zombie reaper is set up as signal handler for SIGCHLD.  This
reaper is inhibited temporarily when we actually want to wait for
child completion (in function run_it) by setting an empty function as
SIGCHLD handler in the master process.  As soon as the child responds
we restore the normal handler. (Note: we do need the dummy, empty
handler as the behaviour of setting SIG_IGN as handler for SIGCHLD is
undefined according to the POSIX standard.)
