CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC = main.c \
      cpu/cpu_load.c \
      governor/governor.c \
      governor/loop.c

OBJ = $(SRC:.c=.o)

TARGET = powergov

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

install:
	cp $(TARGET) /usr/local/bin/

install-service:
	cp service/powergov.service /etc/systemd/system/
	systemctl daemon-reload


clean:
	rm -f $(OBJ) $(TARGET)
