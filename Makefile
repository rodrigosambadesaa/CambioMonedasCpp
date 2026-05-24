APP := progvoraz
SRC := main.c vector_dinamico.c moneda_gestion.c bigint.c
GUI_APP := progvoraz_gui
GUI_SRC_WIN := gui_window.c moneda_gestion.c bigint.c
GUI_SRC_MAC := gui_macos.swift
GUI_SRC_PORTABLE := gui_portable.c moneda_gestion.c bigint.c

CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic
LDFLAGS ?=

BIN_EXT :=
ifeq ($(OS),Windows_NT)
BIN_EXT := .exe
endif

UNAME_S := $(shell uname -s 2>/dev/null)

TARGET := $(APP)$(BIN_EXT)

.PHONY: all debug release run gui run-gui clean help

all: debug

debug: CXXFLAGS += -O0 -g
debug: $(TARGET)

release: CXXFLAGS += -O2
release: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

ifeq ($(OS),Windows_NT)
GUI_TARGET := $(GUI_APP).exe

gui: CXXFLAGS += -O2
gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_SRC_WIN)
	$(CXX) $(CXXFLAGS) $(GUI_SRC_WIN) -o $(GUI_TARGET) $(LDFLAGS) -mwindows

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)
else ifeq ($(UNAME_S),Darwin)
GUI_TARGET := $(GUI_APP)

gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_SRC_MAC)
	swiftc $(GUI_SRC_MAC) -o $(GUI_TARGET)

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)
else
GUI_TARGET := $(GUI_APP)

gui: CXXFLAGS += -O2
gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_SRC_PORTABLE)
	$(CXX) $(CXXFLAGS) $(GUI_SRC_PORTABLE) -o $(GUI_TARGET) $(LDFLAGS)

run-gui: $(GUI_TARGET)
	./$(GUI_TARGET)
endif

clean:
	rm -f progvoraz progvoraz.exe progvoraz_gui progvoraz_gui.exe temporal.exe stock.tmp *.o

help:
	@echo "Targets disponibles:"
	@echo "  make debug    -> compila con simbolos de depuracion"
	@echo "  make release  -> compila optimizado"
	@echo "  make run      -> ejecuta el programa"
	@echo "  make gui      -> compila interfaz GUI (Windows nativa / macOS/Linux portable)"
	@echo "  make run-gui  -> ejecuta interfaz GUI"
	@echo "  make clean    -> elimina artefactos"

