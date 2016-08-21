CC=g++
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
SEL_SRV=selectserver
CLT=client

build: $(SEL_SRV) $(CLT)

$(SEL_SRV):$(SEL_SRV).cpp
	$(CC) $(CCFLAGS) -o $(SEL_SRV) $(LIBSOCKET)  $(CCFLAGS) $(SEL_SRV).cpp

$(CLT):	$(CLT).cpp
	$(CC)  $(CCFLAGS) -o $(CLT) $(LIBSOCKET) $(CLT).cpp

clean:
	rm -f *.o *~
	rm -f $(SEL_SRV) $(CLT)
	rm -f *.log

