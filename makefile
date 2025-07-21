CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic
CCFLAGS = -Iinclude -I/usr/local/include -L/usr/local/lib -lraylib -lGL -lm -g3 -O0

PROJECTNAME = kolibrigame

SRCDIR     = src
GAMESRCDIR = gamesrc
OBJDIR     = obj

TARGET       = $(PROJECTNAME)
CSOURCES     = $(foreach dir,$(SRCDIR), $(notdir $(wildcard $(dir)/*.c)))
GAMECSOURCES = $(foreach dir,$(GAMESRCDIR), $(notdir $(wildcard $(dir)/*.c)))
OBJS         = $(GAMECSOURCES:%.c=$(OBJDIR)/%.o)

all: rm-elf prepare $(TARGET)

clean:
	rm -rf $(OBJDIR)

rm-elf:
	rm -f $(TARGET)

$(OBJDIR)/%.o: $(GAMESRCDIR)/%.c
	$(CC) $(CFLAGS) $(CCFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS) $(CCFLAGS)

prepare:
	mkdir -p $(OBJDIR)
