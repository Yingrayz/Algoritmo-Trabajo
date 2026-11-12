CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2
LDFLAGS = -lm
TARGET  = artemis_sls
OBJS    = main.o artemis.o validaciones.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c artemis.h validaciones.h
artemis.o: artemis.c artemis.h
validaciones.o: validaciones.c validaciones.h

run: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all run clean
