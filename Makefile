CC = clang
CFLAGS = -Wall -Wextra -std=c11 -O2
PKG_CONFIG = pkg-config
GTK4_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk4 libsoup-3.0 json-glib-1.0)
GTK4_LIBS = $(shell $(PKG_CONFIG) --libs gtk4 libsoup-3.0 json-glib-1.0)

TARGET = weatherclock
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(GTK4_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(GTK4_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) $(TARGET).exe

install: $(TARGET)
	@echo "Build complete! Run ./$(TARGET) or ./$(TARGET).exe"

