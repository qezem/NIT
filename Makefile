CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LIBS = -lcrypto

TARGET = nit
OBJ_DIR = ./Obj

SRC = $(wildcard *.c) $(wildcard */*.c)
OBJ = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRC))

$(TARGET) : $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/%.o : %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -rf $(TARGET) $(OBJ_DIR)