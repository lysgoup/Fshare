all: client/fshare.c server/fshared.c
	gcc -o client/fshare client/fshare.c -DMAIN
	gcc -o server/fshared server/fshared.c -DMAIN -lpthread

clean:
	rm client/fshare server/fshared