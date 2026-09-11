CC := gcc
CFLAGS := -Wall -Wextra -std=c99 -O2

TARGET := server
SRC := server.c
OBJ := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET) 5000

.PHONY: all clean run
