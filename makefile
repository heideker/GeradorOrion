# for raspbian, include libcurl4-openssl-dev package
all: 
	gcc -Wall -o clienteOrion client.cpp -lstdc++ -lcurl -lpthread -std=c++11 -lm

