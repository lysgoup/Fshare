all: client.c server.c
	gcc -o client client.c -lpthread
	gcc -o server server.c -lpthread

clean:
	rm client server