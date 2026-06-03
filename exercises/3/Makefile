# Default macros:
# $@ - Target name
# $? - All CHANGED dependencies
# $< - First dependency
# $^ - All dependencies
# $(CC) - Default C compiler (usually gcc)
# @<command> - Suppresses the command from being printed to the terminal `@echo` in this file

EXEC = program
OBJECT_FILES = math_ops.o main.o

# DEBUG_FLAG = -g # Uncomment the above line to enable debugging symbols
CFLAGS = $(DEBUG_FLAG) -std=c99 -Wall

# Compile the whole program
$(EXEC): $(OBJECT_FILES)
	@echo "[3/3] Linking the executable"
	$(CC) $^ -o $@

# Compiling the object files as thiers dependencies
main.o: main.c math_ops.h
	@echo "[2/3] Compiling the main.o"
	$(CC) -c $(CFLAGS) $< -o $@

math_ops.o: math_ops.c math_ops.h
	@echo "[1/3] Compiling the math_ops.o"
	$(CC) -c $(CFLAGS) $< -o $@

# Clean all of the build artifacts
clean:
	rm -f $(OBJECT_FILES) $(EXEC)
