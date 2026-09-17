PREFIX=/usr/local

CC = clang
TARGET = openphoto
SOURCE = openphoto.c

CFLAGS = -Wall -Wextra -pedantic -O2
LDFLAGS = -lX11 -lpng

# UNCOMMENT ON OPENBSD
CFLAGS += -I /usr/X11R6/include -I /usr/local/include
LDFLAGS += -L /usr/X11R6/lib -L /usr/local/lib
all:
	$(CC) $(SOURCE) $(CFLAGS) $(LDFLAGS) -o $(TARGET)
test:
	make all && ./$(TARGET) ~/pictures/openbsd/openbased.png 
grayscale:
	make all && ./$(TARGET) ~/pictures/openbsd/openbased.png -g
