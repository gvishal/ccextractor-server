SHELL = /bin/sh

CC = gcc
# CFLAGS = -Wall -Wextra -g
# ALL_FLAGS = 

TARGET = ../../server

OBJS_DIR = objs

SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=$(OBJS_DIR)/%.o)

.PHONY: all
all: objs_dir $(TARGET) 

.PHONY: objs_dir
objs_dir: 
	mkdir -p $(OBJS_DIR)

$(TARGET): $(OBJS) 
	$(CC) $(ALL_FLAGS) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(OBJS_DIR)/%.o: %.c
	$(CC) -c $(ALL_FLAGS) $(CFLAGS) $< -o $@  

.PHONY: clean
clean:
	rm $(TARGET) 2>/dev/null || true
	rm $(OBJS) 2>/dev/null || true
	rm -rd $(OBJS_DIR) 2>/dev/null || true
	rm .depend 2>/dev/null || true

.PHONY: depend dep
depend dep:
	$(CXX) -E -MM $(SRCS) |\
		sed 's/^[a-zA-Z_0-9]*.o/$(OBJS_DIR)\/&/' > .depend

-include .depend
