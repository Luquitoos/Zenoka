CC = gcc
PKGCONFIG = pkg-config
CFLAGS = -Wall -Wextra -g -Iinclude $(shell $(PKGCONFIG) --cflags gtk4)
LIBS = $(shell $(PKGCONFIG) --libs gtk4)

SRC_DIR = src
BUILD_DIR = build
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = Zenoka  # Executável na raiz do projeto

ifeq ($(OS),Windows_NT)
	MKDIR = if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	RM = rmdir /S /Q $(BUILD_DIR) 2>nul || del $(BUILD_DIR)\*.o 2>nul
	EXEC = $(TARGET).exe
else
	MKDIR = mkdir -p $(BUILD_DIR)
	RM = rm -rf $(BUILD_DIR)
	EXEC = ./$(TARGET)
endif

all: $(TARGET)

$(BUILD_DIR):
	$(MKDIR)

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM)
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean run debug
