CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -DPOWERGOV_VERSION=\"$(VERSION)\"
VERSION := $(shell cat VERSION)

SRC = main.c \
      src/version.c \
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
ZSH_COMP = completions/zsh/_powergov
MAN1_DIR = /usr/local/share/man/man1
MAN8_DIR = /usr/local/share/man/man8
BASH_COMP_DIR = /usr/share/bash-completion/completions
BASH_COMP_LEGACY_DIR = /etc/bash_completion.d
ZSH_COMP_DIR = /usr/local/share/zsh/site-functions
DIST_NAME = powergov-$(VERSION)
DIST_DIR = dist/$(DIST_NAME)
DIST_TAR = dist/$(DIST_NAME).tar.gz
PACK_FILES = VERSION Makefile README.md main.c src include governor cpu Battery config completions doc service .gitignore

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
	install -D -m 644 $(BASH_COMP) $(BASH_COMP_LEGACY_DIR)/powergov
	install -D -m 644 $(ZSH_COMP) $(ZSH_COMP_DIR)/_powergov

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
	rm -f $(MAN1_DIR)/powergov.1 $(MAN8_DIR)/powergov.8
	rm -f $(BASH_COMP_DIR)/powergov $(BASH_COMP_LEGACY_DIR)/powergov $(ZSH_COMP_DIR)/_powergov
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

pack: clean
	@mkdir -p $(DIST_DIR)
	@cp -r $(PACK_FILES) $(DIST_DIR)/
	@mkdir -p dist
	@tar -czf $(DIST_TAR) -C dist $(DIST_NAME)
	@echo "packed $(DIST_TAR)"

extract:
	@test -f $(DIST_TAR) || (echo "missing $(DIST_TAR); run make pack first" && exit 1)
	@rm -rf $(DIST_DIR)
	@tar -xzf $(DIST_TAR) -C dist
	@echo "extracted to $(DIST_DIR)"

.PHONY: all install install-bin install-man install-completion install-service stop uninstall uninstall-docs uninstall-service service-status pack extract clean

clean:
	rm -f $(OBJ) $(TARGET)
