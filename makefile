# Library name
LIBNAME = kolibri
VERSION = 0.1.0

# Compiler
CC = gcc-14
AR = ar
ARFLAGS = rcs

# Auto-detect platform
PLATFORM := $(shell uname -s)

# Platform-specific adjustments
ifeq ($(PLATFORM),Darwin)
    # macOS
endif
ifeq ($(findstring MINGW,$(PLATFORM)),MINGW)
    PLATFORM = mingw
endif
ifeq ($(findstring CYGWIN,$(PLATFORM)),CYGWIN)
    PLATFORM = cygwin
endif

# Allow manual override: make PLATFORM_OVERRIDE=dc
ifdef PLATFORM_OVERRIDE
    PLATFORM = $(PLATFORM_OVERRIDE)
endif

# Directories
SRCDIR  = src
INCDIR  = include
OBJDIR  = obj/$(PLATFORM)
LIBDIR  = lib
INSTALLDIR = /usr/local

# Output libraries
LIB_RELEASE = $(LIBDIR)/lib$(LIBNAME).a
LIB_DEBUG   = $(LIBDIR)/lib$(LIBNAME)_debug.a

# Source files
SRCS = $(wildcard $(SRCDIR)/*.c)

# Object files
OBJS_RELEASE = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/release/%.o)
OBJS_DEBUG   = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/debug/%.o)

# Compiler flags
CFLAGS_COMMON = -I$(INCDIR) -Wall -Wextra -Wpedantic
CFLAGS_RELEASE = $(CFLAGS_COMMON) -O3 -DNDEBUG
CFLAGS_DEBUG = $(CFLAGS_COMMON) -g3 -O0 -DDEBUG

.PHONY: all clean install uninstall release debug info

# Default target builds both versions
all: release debug

# Release build
release: $(LIB_RELEASE)

$(LIB_RELEASE): $(OBJS_RELEASE)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $^
	@echo "Built release library: $@"

$(OBJDIR)/release/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)/release
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

# Debug build
debug: $(LIB_DEBUG)

$(LIB_DEBUG): $(OBJS_DEBUG)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $^
	@echo "Built debug library: $@"

$(OBJDIR)/debug/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)/debug
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(LIBDIR) $(OBJDIR)

# Install headers and libraries
install: all
	@echo "Installing headers to $(INSTALLDIR)/include/$(LIBNAME)/"
	install -d $(INSTALLDIR)/include/$(LIBNAME)
	install -m 644 $(INCDIR)/*.h $(INSTALLDIR)/include/$(LIBNAME)/
	@echo "Installing libraries to $(INSTALLDIR)/lib/"
	install -d $(INSTALLDIR)/lib
	install -m 644 $(LIB_RELEASE) $(INSTALLDIR)/lib/
	install -m 644 $(LIB_DEBUG) $(INSTALLDIR)/lib/
	@echo "Installation complete"
	@echo ""
	@echo "To use in your project:"
	@echo "  CFLAGS += -I$(INSTALLDIR)/include"
	@echo "  LDFLAGS += -L$(INSTALLDIR)/lib -l$(LIBNAME)        # Release"
	@echo "  LDFLAGS += -L$(INSTALLDIR)/lib -l$(LIBNAME)_debug  # Debug"

# Uninstall
uninstall:
	rm -rf $(INSTALLDIR)/include/$(LIBNAME)
	rm -f $(INSTALLDIR)/lib/lib$(LIBNAME).a
	rm -f $(INSTALLDIR)/lib/lib$(LIBNAME)_debug.a
	@echo "Uninstallation complete"

# Show build info
info:
	@echo "Library:  $(LIBNAME) v$(VERSION)"
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CC)"
	@echo "Sources:  $(SRCS)"
	@echo ""
	@echo "Build outputs:"
	@echo "  Release: $(LIB_RELEASE)"
	@echo "  Debug:   $(LIB_DEBUG)"
	@echo ""
	@echo "Object directories:"
	@echo "  Release: $(OBJDIR)/release/"
	@echo "  Debug:   $(OBJDIR)/debug/"
