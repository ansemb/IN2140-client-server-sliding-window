# Go-back-n protocol on top of UDP
## Intro
This is a client-server application that compares images that are stored on two end systems. The client-server creates layer on top of UDP that provides flow control and loss recovery by using the Sliding window algorithm go-back-n. Go-back-n is an mediocre flow control/loss recovery algorithm if the loss percentage is low. This repo shows an example implementation of this algorithm.

## Compiling
```bash
make all
```
compiles both client and server.

## Running

#### example run server
```bash
./server 8080 srv_folder out.txt
```
This will run the server, accepting connection on port 8080, use the image directory called "./srv_folder" and write output to "out.txt".

#### example run client
```bash
./client localhost 8080 list_of_filenames.txt 15
```
This will run the client, connecting to localhost on port 8080, search the file "list_of_filenames.txt" for image files, and send them to the server with a drop percentage of 15%.