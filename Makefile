CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -pthread -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS := -pthread

TARGET := sim
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) --mode seq --trials 100000 --steps 50 --seed 42 --metrics-detail 0

test: $(TARGET)
	scripts/test_modes.sh

clean:
	rm -f $(TARGET) src/*.o
