# Define the target executable name
TARGET = memctl_ioctl

# Define the C compiler
CC = clang

# Define the C compiler flags
CFLAGS = -Wall -g

# Define all .c source files
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

# Define all .h header files
HEADERS = $(wildcard *.h)

# Default rule to build the target executable
default: $(TARGET)

# Rule to compile individual object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link the object files into the final executable
$(TARGET): $(OBJECTS)
	$(CC) -static $(OBJECTS) $(LIBS) -o $@

# Clean rule to remove all object files and the executable
clean:
	-rm -f $(OBJECTS) $(TARGET)

# Define a phony target for "all" to make it always rebuild
.PHONY: all clean
