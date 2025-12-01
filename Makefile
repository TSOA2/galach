CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Werror
LFLAGS :=

ifeq ($(MODE), prod)
	CFLAGS += -O3
else
	CFLAGS += -O0 -g -DDEBUG
endif

ifeq ($(MODE), test)
	CFLAGS += -DTEST
endif

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o, $(SRCS))
OUT  := galach

all: $(OUT)
run: $(OUT)
	./$(OUT)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f $(OUT) $(OBJS)

