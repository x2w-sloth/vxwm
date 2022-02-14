CFLAGS  := -Wall -Wextra -Wpedantic -std=c17 -O2
LDFLAGS := -lxcb -lxcb-keysyms -lxcb-icccm
SRCDIR  := src
INSDIR  := /usr/local/bin
SRC     := $(wildcard $(SRCDIR)/*.c)
OBJ     := $(patsubst $(SRCDIR)/%.c, %.o, $(SRC))

all: clean vxwm

%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

vxwm: clean $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

debug: CFLAGS += -DVXWM_DEBUG
debug: clean vxwm

clean:
	rm -f *.o vxwm

install: all
	mkdir -p $(INSDIR)
	cp -f vxwm $(INSDIR)
	chmod 755 $(INSDIR)/vxwm

uninstall:
	rm -f $(INSDIR)/vxwm

.PHONY: all vxwm debug clean install uninstall
