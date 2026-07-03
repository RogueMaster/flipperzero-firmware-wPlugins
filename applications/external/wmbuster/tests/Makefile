# Host-side tests. Run with `make -C tests`.
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 -I..
SRC      = test_link.c \
           ../protocol/wmbus_link.c \
           ../protocol/wmbus_manuf.c \
           ../protocol/wmbus_medium.c

test_link: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@

IZAR_SRC = test_izar.c \
           ../drivers/europe/diehl/izar.c \
           ../drivers/engine/driver.c \
           ../protocol/wmbus_link.c \
           ../protocol/wmbus_manuf.c \
           ../protocol/wmbus_medium.c \
           ../protocol/wmbus_app_layer.c

test_izar: $(IZAR_SRC)
	$(CC) $(CFLAGS) $(IZAR_SRC) -o $@

PORTS_SRC = test_ports.c \
            ../drivers/europe/bmeters/hydrodigit.c \
            ../drivers/europe/engelmann/hydroclima.c \
            ../drivers/europe/sontex/rfmtx1.c \
            ../drivers/europe/zenner/zenner0b.c \
            ../drivers/europe/techem/mkradio3a.c \
            ../drivers/europe/misc/gwfwater.c \
            ../drivers/engine/driver.c \
            ../protocol/wmbus_link.c \
            ../protocol/wmbus_manuf.c \
            ../protocol/wmbus_medium.c \
            ../protocol/wmbus_app_layer.c

test_ports: $(PORTS_SRC)
	$(CC) $(CFLAGS) $(PORTS_SRC) -o $@

check: test_link test_izar test_ports
	./test_link
	./test_izar
	./test_ports

clean:
	rm -f test_link test_izar test_ports

.PHONY: check clean
