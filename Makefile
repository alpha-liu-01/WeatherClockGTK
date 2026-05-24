CC = clang
CFLAGS = -Wall -Wextra -std=c11 -O2 -Isrc
PKG_CONFIG = pkg-config
GTK4_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk4 libsoup-3.0 json-glib-1.0)
GTK4_LIBS = $(shell $(PKG_CONFIG) --libs gtk4 libsoup-3.0 json-glib-1.0)

TARGET = weatherclock
SOURCES = src/main.c src/clock.c src/weather.c src/config.c src/ui.c src/i18n.c
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
	@echo "Use CMake to install system-wide: sudo cmake --install build"
	@echo "Or run from the build tree: ./$(TARGET)"
