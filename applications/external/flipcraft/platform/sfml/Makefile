CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SFML_CFLAGS ?= $(shell pkg-config --cflags sfml-graphics sfml-window sfml-system 2>/dev/null)
SFML_LIBS ?= $(shell pkg-config --libs sfml-graphics sfml-window sfml-system 2>/dev/null)
ifeq ($(strip $(SFML_LIBS)),)
SFML_CFLAGS := -I/opt/homebrew/include -I/usr/local/include
SFML_LIBS := -L/opt/homebrew/lib -L/usr/local/lib -lsfml-graphics -lsfml-window -lsfml-system
endif

APP := flipcraft-sfml
ROOT := ../..
SOURCES := \
	main.cpp \
	platform.cpp \
	$(ROOT)/game/game.cpp \
	$(ROOT)/game/data.cpp \
	$(ROOT)/game/world.cpp \
	$(ROOT)/render/render.cpp \
	$(ROOT)/render/gui.cpp

$(APP): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SFML_CFLAGS) -I$(ROOT) $(SOURCES) -o $(APP) $(SFML_LIBS)

.PHONY: clean
clean:
	rm -f $(APP)
