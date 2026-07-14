CC ?= gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c11 -D_GNU_SOURCE
LDFLAGS =

# Source files
SRCS = main.c wfr_lirc.c wfr_evdev.c wfr_decode.c wfr_network.c wfr_ack.c ../flipper/protocol/wfr_protocol.c
OBJS = $(SRCS:.c=.o)
TARGET = infrafid

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -I.. -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 infrafid.service $(DESTDIR)/etc/systemd/system/infrafid.service
	@echo "Installed. Run: systemctl daemon-reload && systemctl enable --now infrafid"
