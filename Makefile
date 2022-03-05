CFLAGS  := -Wall -Wextra -Wpedantic -std=c17 -O2
LDFLAGS := `pkg-config --libs xcb xcb-keysyms xcb-icccm cairo`
SRCDIR  := src
INSDIR  := /usr/local/bin
SRC     := $(wildcard $(SRCDIR)/*.c)
OBJ     := $(patsubst $(SRCDIR)/%.c, %.o, $(SRC))

all: vxwm

%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

vxwm: LDFLAGS += -s
vxwm: clean $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

debug: CFLAGS += -DVXWM_DEBUG -g
debug: clean $(OBJ)
	$(CC) $(OBJ) -o vxwm $(LDFLAGS)

clean:
	rm -f *.o vxwm

install: all
	mkdir -p $(INSDIR)
	cp -f vxwm $(INSDIR)
	chmod 755 $(INSDIR)/vxwm

uninstall:
	rm -f $(INSDIR)/vxwm

.PHONY: all vxwm debug clean install uninstall
