# Makefile for Traceroute Implementation
# Cross-platform build system

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2

# Platform detection
ifeq ($(OS),Windows_NT)
    # Windows specific
    LDFLAGS = -lws2_32
    RM = del /Q
    EXE_EXT = .exe
    TARGET_SIMPLE = traceroute_simple.exe
    TARGET_FULL = traceroute.exe
else
    # Linux/Mac
    LDFLAGS =
    RM = rm -f
    EXE_EXT =
    TARGET_SIMPLE = traceroute_simple
    TARGET_FULL = traceroute
endif

# Source files
SRC_SIMPLE = traceroute_simple.c
SRC_FULL = traceroute.c

# Default target - build everything
all: $(TARGET_SIMPLE) $(TARGET_FULL)
	@echo "Build complete!"
	@echo "Run: $(TARGET_SIMPLE) google.com"
	@echo "     $(TARGET_FULL) google.com -m 20"

# Build simple version
$(TARGET_SIMPLE): $(SRC_SIMPLE)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Build full version
$(TARGET_FULL): $(SRC_FULL)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Debug build
debug: CFLAGS = -Wall -Wextra -g -DDEBUG
debug: clean all
	@echo "Debug build complete!"

# Clean build artifacts
clean:
ifeq ($(OS),Windows_NT)
	-$(RM) *.exe *.obj 2>nul
else
	-$(RM) $(TARGET_SIMPLE) $(TARGET_FULL) *.o
endif
	@echo "Cleaned build artifacts"

# Install (Linux/Mac only)
install: all
ifndef OS  # Not Windows
	sudo cp $(TARGET_SIMPLE) /usr/local/bin/
	sudo cp $(TARGET_FULL) /usr/local/bin/
	@echo "Installed to /usr/local/bin/"
endif

# Uninstall (Linux/Mac only)
uninstall:
ifndef OS  # Not Windows
	sudo rm -f /usr/local/bin/$(TARGET_SIMPLE)
	sudo rm -f /usr/local/bin/$(TARGET_FULL)
	@echo "Uninstalled from /usr/local/bin/"
endif

# Help
help:
	@echo "Traceroute Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build all targets (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  debug      - Build with debug symbols"
	@echo "  install    - Install to system (Linux/Mac only)"
	@echo "  uninstall  - Remove from system (Linux/Mac only)"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build everything"
	@echo "  make clean        # Clean build"
	@echo "  make debug        # Debug build"
	@echo ""
	@echo "Platform: $(shell uname -s 2>/dev/null || echo Windows)"

.PHONY: all clean debug install uninstall help
