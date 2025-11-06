CC      = gcc-14
CFLAGS  = -Wall -Wextra -Wpedantic -g3 -O0 -DDEBUG #-DHEAD_USE_RENDER_TEXTURE
LIBS    = -lraylib -lGL -lm
INCLUDE = -I../../include -I/usr/local/include -I../
LIBDIR  = -L/usr/local/lib

PROJECTNAME = fps_example

# Auto-detect platform using uname directly
PLATFORM := $(shell uname -s)

# Platform-specific adjustments
ifeq ($(PLATFORM),Darwin)
    LIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(findstring MINGW,$(PLATFORM)),MINGW)
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
    TARGET_EXT = .exe
endif
ifeq ($(findstring CYGWIN,$(PLATFORM)),CYGWIN)
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
    TARGET_EXT = .exe
endif

# Allow manual override: make PLATFORM=dc
ifdef PLATFORM_OVERRIDE
    PLATFORM = $(PLATFORM_OVERRIDE)
endif

SRCDIR      = ../../src
EXAMPLESDIR = ../
GAMESRCDIR  = .
OBJDIR      = obj/$(PLATFORM)

TARGET       = $(PROJECTNAME)$(TARGET_EXT)

ENGINE_SRC   = $(wildcard $(SRCDIR)/*.c)
EXAMPLES_SRC = $(wildcard $(EXAMPLESDIR)/*.c)
GAME_SRC     = $(wildcard $(GAMESRCDIR)/*.c)

ENGINE_OBJS   = $(ENGINE_SRC:$(SRCDIR)/%.c=$(OBJDIR)/engine/%.o)
EXAMPLES_OBJS = $(EXAMPLES_SRC:$(EXAMPLESDIR)/%.c=$(OBJDIR)/game/%.o)
GAME_OBJS     = $(GAME_SRC:$(GAMESRCDIR)/%.c=$(OBJDIR)/game/%.o)

OBJS = $(ENGINE_OBJS) $(EXAMPLES_OBJS) $(GAME_OBJS)

.PHONY: all clean rm-elf prepare

all: rm-elf prepare $(TARGET)

clean: rm-elf
	rm -rf $(OBJDIR)

rm-elf:
	rm -f $(TARGET)

prepare:
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/engine
	mkdir -p $(OBJDIR)/game

$(OBJDIR)/engine/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
	
$(OBJDIR)/game/%.o: $(EXAMPLESDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(OBJDIR)/game/%.o: $(GAMESRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBDIR) $(LIBS)

debug:
	@echo "DETECTED PLATFORM: $(PLATFORM)"
	@echo "TARGET:         $(TARGET)"
	@echo "ENGINE_SOURCES: $(ENGINE_SRC)"
	@echo "GAME_SOURCES:   $(GAME_SRC)"
	@echo "ENGINE_OBJS:    $(ENGINE_OBJS)"
	@echo "GAME_OBJS:      $(GAME_OBJS)"
	@echo "OBJS:           $(OBJS)"
