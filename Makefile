CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRC = src/ls-v1.0.0.c
OUT = bin/ls

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)

.PHONY: all clean
