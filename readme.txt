This is a webserver application written in c used to serve files to the tcp socket.

This webserver was written to run on a CentOS environment

Dowload webapp.tar and extract the files into a Unix directory.
You should see the following files:

webserver.c -- C code for a multi-threaded web server
nethelp.c -- File containing helper networking functions
nethelp.h -- Header file for helper networking functions defined in nethelp.c
Makefile -- Compiles and links together the source files
ws.conf -- an example config file
readme -- this file

1. Build the webserver executable by typing in
make at the shell prompt.

2. Configure the config file to match your document root

3. Run the web server from the command line
	Pass your configfile location as an argument to the server:

     	./webserver <config file location>

4. You can now make requests to the Server to test it try running the following command on a seperate terminal.
(echo -en "GET /index.html HTTP/1.1\n Host: localhost \nConnection: keep-alive\n\nGET
/index.html HTTP/1.1\nHost: localhost\n\n"; sleep 10) | telnet localhost 80
