CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -DPOWERGOV_VERSION=\"$(VERSION)\"
LDFLAGS =
VERSION := $(shell cat VERSION)

PKG_CONFIG ?= pkg-config
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 2>/dev/null)

SRC = main.c \
      src/version.c \
      include/powergov/types.c \
      core/sysfs.c \
      core/state_machine.c \
      core/loop.c \
      core/info.c \
      cpu/cpu_load.c \
      cpu/governor.c \
      cpu/epp.c \
      cpu/freq_cap.c \
      cpu/turbo.c \
      cpu/policy.c \
      power/power_supply.c \
      power/profile.c \
      platform/platform_profile.c \
      devices/runtime_pm.c \
      log/log.c \
      metrics/metrics.c \
      config/config.c

OBJ = $(SRC:.c=.o)

TARGET = powergov
LIBPOWERGOV = libpowergov.so
UI_BIN = powergov-ui
POLKIT_POLICY = data/org.powergov.policy
POLKIT_DIR = /usr/share/polkit-1/actions
DEV_AUTH = scripts/powergov-dev-auth
LIBEXEC_DIR = /usr/local/libexec/powergov
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
ICON_ROOT = /usr/share/icons/hicolor
DESKTOP_FILE = data/powergov-ui.desktop
DESKTOP_DIR = /usr/share/applications
PACK_DIRS = VERSION Makefile README.md main.c src include core cpu power platform devices log metrics config client data Documentation completions doc service scripts .gitignore
PACK_UI = ui/main.c ui/README.md

LIB_OBJ = client/libpowergov.o include/powergov/types_lib.o

include/powergov/types_lib.o: include/powergov/types.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

$(LIBPOWERGOV): $(LIB_OBJ)
	$(CC) -shared -fPIC $(CFLAGS) -o $(LIBPOWERGOV) $(LIB_OBJ) $(LDFLAGS)

client/%.o: client/%.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(UI_BIN): ui/main.c $(LIBPOWERGOV)
	@test -n "$(GTK_LIBS)" || (echo "gtk+-3.0 no encontrado (pkg-config)" && exit 1)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -o $(UI_BIN) ui/main.c \
		-L. -lpowergov -Wl,-rpath,'$$ORIGIN' $(GTK_LIBS)

install: install-bin install-man install-completion install-lib install-ui-policy

install-lib: $(LIBPOWERGOV)
	install -D -m 755 $(LIBPOWERGOV) /usr/local/lib/$(LIBPOWERGOV)
	install -D -m 644 include/powergov/client.h /usr/local/include/powergov/client.h
	-@if command -v ldconfig >/dev/null 2>&1; then ldconfig; fi

install-ui: $(UI_BIN) install-lib install-ui-policy install-ui-helper install-ui-icons install-ui-desktop
	install -D -m 755 $(UI_BIN) /usr/local/bin/$(UI_BIN)

install-ui-icons:
	@for sz in 16 24 32 48 64 128 256; do \
		src="data/icons/hicolor/$${sz}x$${sz}/apps/powergov.png"; \
		dst="$(ICON_ROOT)/$${sz}x$${sz}/apps/powergov.png"; \
		if [ -f "$$src" ]; then install -D -m 644 "$$src" "$$dst"; fi; \
	done
	@install -D -m 644 data/icons/hicolor/scalable/apps/powergov.svg \
		$(ICON_ROOT)/scalable/apps/powergov.svg
	-@if command -v gtk-update-icon-cache >/dev/null 2>&1; then \
		gtk-update-icon-cache -f -t $(ICON_ROOT) || true; \
	fi

install-ui-desktop:
	install -D -m 644 $(DESKTOP_FILE) $(DESKTOP_DIR)/powergov-ui.desktop
	-@if command -v update-desktop-database >/dev/null 2>&1; then \
		update-desktop-database $(DESKTOP_DIR) || true; \
	fi

install-ui-helper:
	install -D -m 755 $(DEV_AUTH) $(LIBEXEC_DIR)/dev-auth

install-ui-policy:
	install -D -m 644 $(POLKIT_POLICY) $(POLKIT_DIR)/org.powergov.policy

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

pack: clean icons
	@mkdir -p $(DIST_DIR)
	@cp -r $(PACK_DIRS) $(DIST_DIR)/
	@mkdir -p $(DIST_DIR)/ui
	@cp $(PACK_UI) $(DIST_DIR)/ui/
	@mkdir -p dist
	@tar -czf $(DIST_TAR) -C dist $(DIST_NAME)
	@echo "packed $(DIST_TAR)"

icons:
	python3 scripts/render-icons.py

extract:
	@test -f $(DIST_TAR) || (echo "missing $(DIST_TAR); run make pack first" && exit 1)
	@rm -rf $(DIST_DIR)
	@tar -xzf $(DIST_TAR) -C dist
	@echo "extracted to $(DIST_DIR)"

release:
	@./scripts/release.sh

ui: $(UI_BIN)

ui-run: $(UI_BIN)
	./$(UI_BIN)

.PHONY: all install install-bin install-man install-completion install-service install-lib install-ui install-ui-helper install-ui-policy install-ui-icons install-ui-desktop icons stop uninstall uninstall-docs uninstall-service service-status pack extract release ui ui-run clean

clean:
	rm -f $(OBJ) client/libpowergov.o include/powergov/types_lib.o $(TARGET) $(LIBPOWERGOV) $(UI_BIN)
