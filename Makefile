CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC = main.c \
      cpu/cpu_load.c \
      governor/governor.c \
      governor/loop.c \
	  Battery/battery.c

OBJ = $(SRC:.c=.o)

TARGET = powergov

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

install:
	cp $(TARGET) /usr/local/bin/
	-@if command -v systemctl >/dev/null 2>&1 && systemctl list-unit-files --type=service 2>/dev/null | grep -q '^powergov\.service'; then \
		systemctl restart powergov.service; \
	fi

install-service:
	cp service/powergov.service /etc/systemd/system/
	systemctl daemon-reload

stop:
	-pkill -x $(TARGET)

uninstall: stop uninstall-service
	rm -f /usr/local/bin/$(TARGET)

uninstall-service:
	-systemctl stop powergov.service
	-systemctl disable powergov.service
	rm -f /etc/systemd/system/powergov.service
	-systemctl daemon-reload

service-status:
	-@if command -v systemctl >/dev/null 2>&1; then \
		systemctl --no-pager status powergov.service || true; \
	else \
		echo "systemctl not available"; \
	fi


clean:
	rm -f $(OBJ) $(TARGET)
