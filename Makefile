all: client/fshare.c server/fshared.c
	gcc -o client/fshare client/fshare.c -DMAIN
	gcc -o server/fshared server/fshared.c -DMAIN -lpthread

debug:
	gcc -o client/fshare client/fshare.c -DMAIN -DDEBUG
	gcc -o server/fshared server/fshared.c -DMAIN -lpthread -DDEBUG

clean:
	rm client/fshare server/fshared
