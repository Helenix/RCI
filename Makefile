all: reqserv service

reqserv: reqserv.c
	gcc reqserv.c -o reqserv 

service: service.c
	gcc service.c -o service

clean:
	rm *.o