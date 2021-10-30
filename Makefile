CC = gcc
CXX = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv`
LINK_PTHREAD = -lpthread

CLIENT = client.cpp
SERVER = server.cpp
OPEN_CV = openCV.cpp
PTHREAD = pthread.c
CLI = client
SER = server
CV = openCV
PTH = pthread

all: server client opencv pthread
  
server: $(SERVER) msg.hpp
	$(CXX) $(SERVER) -o $(SER) -std=c++17
client: $(CLIENT) msg.hpp
	$(CXX) $(CLIENT) -o $(CLI) -std=c++17
opencv: $(OPEN_CV)
	$(CXX) $(OPEN_CV) -o $(CV) $(INCLUDE_OPENCV)
pthread: $(PTHREAD)
	$(CC) $(PTHREAD) -o $(PTH) $(LINK_PTHREAD)

.PHONY: clean

clean:
	rm $(CLI) $(SER) $(CV) $(PTH)

clean-folder:
	rm -r b08902040*

create-file:
	echo abc > b08902040_1_client_folder/file1
	echo nihao > b08902040_1_client_folder/file3
	echo nihaoah > b08902040_1_client_folder/file4

rm-file:
	rm b08902040_1_client_folder/*
