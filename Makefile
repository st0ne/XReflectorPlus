CC = gcc
CXX = g++

FLAGS = -g -W -Wall -D_GNU_SOURCE -lrt -pthread -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security
CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS)

OBJECTS = dxrfd.o

LIBS = -lstdc++ -lrt -lpthread

all: clean build

build: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) $(FLAGS) -o dxrfd

clean:
	rm -f *.o dxrfd
