ccextractor-server
==================
ccextractor-server provides a server part of closed captions (CC) online repository
wich allows to receive, store and show CC in real-time. 

It works as follow: 
1. Client uses CCExtractor (https://github.com/CCExtractor/ccextractor) -- a program that can 
extract CC from video stream -- to read video stream and to send extracted CC data (in its own format) to the server.
2. Server receives that data and converts it to CC text. 
3. This CC text is then stored in the data base and can be viewed real-time using web site.

Building and running
==================
1. Run `make` to build ccextractor-server (make sure you have MySQL cliet library (lmysqlclient) installed)
2. Compile CCExtractor and place it to server directory
3. Specify MySQL user and password in configuration file (server.ini). 
ccextractor-server will automatically create data base and neccessary tables.
4. To use it with web site, place ccextractor-server files in your web server repository
5. Run `./server` to start the server. By default, it'll listen to 2048 port.

