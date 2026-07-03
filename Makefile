CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC = main.c \
      cpu/cpu_load.c \
      governor/governor.c \
      governor/loop.c \
      Battery/battery.c \
      config/config.c

OBJ = $(SRC:.c=.o)

TARGET = powergov
CONF_DIR = /etc
CONF_FILE = $(CONF_DIR)/powergov.conf
SYSTEMD_UNIT = /etc/systemd/system/powergov.service
MAN1 = doc/powergov.1
MAN8 = doc/powergov.8
BASH_COMP = completions/bash/powergov
MAN1_DIR = /usr/local/share/man/man1
MAN8_DIR = /usr/local/share/man/man8
BASH_COMP_DIR = /usr/share/bash-completion/completions

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

install: install-bin install-man install-completion

install-bin:
	install -D -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	-@if command -v systemctl >/dev/null 2>&1 && systemctl is-enabled powergov.service >/dev/null 2>&1; then \
		systemctl restart powergov.service; \
	fi

install-man:
	install -D -m 644 $(MAN1) $(MAN1_DIR)/powergov.1
	install -D -m 644 $(MAN8) $(MAN8_DIR)/powergov.8
	-@if command -v mandb >/dev/null 2>&1; then mandb -q; fi

install-completion:
	install -D -m 644 $(BASH_COMP) $(BASH_COMP_DIR)/powergov

install-service: install
	install -D -m 644 config/powergov.conf $(CONF_FILE)
	install -D -m 644 service/powergov.service $(SYSTEMD_UNIT)
	systemctl daemon-reload
	systemctl enable powergov.service
	systemctl restart powergov.service

stop:
	-@if command -v systemctl >/dev/null 2>&1 && systemctl is-active powergov.service >/dev/null 2>&1; then \
		systemctl stop powergov.service; \
	else \
		pkill -x $(TARGET) || true; \
	fi

uninstall: stop uninstall-service uninstall-docs
	rm -f /usr/local/bin/$(TARGET)

uninstall-docs:
	rm -f $(MAN1_DIR)/powergov.1 $(MAN8_DIR)/powergov.8 $(BASH_COMP_DIR)/powergov
	-@if command -v mandb >/dev/null 2>&1; then mandb -q; fi

uninstall-service:
	-systemctl stop powergov.service
	-systemctl disable powergov.service
	rm -f $(SYSTEMD_UNIT)
	-systemctl daemon-reload

service-status:
	-@if command -v systemctl >/dev/null 2>&1; then \
		systemctl --no-pager status powergov.service || true; \
	else \
		echo "systemctl not available"; \
	fi

.PHONY: all install install-bin install-man install-completion install-service stop uninstall uninstall-docs uninstall-service service-status clean

clean:
	rm -f $(OBJ) $(TARGET)
