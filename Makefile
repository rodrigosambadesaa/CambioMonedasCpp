APP := progvoraz
SRC := main.cpp portable_cli.cpp vector_dinamico.cpp moneda_gestion.cpp bigint.cpp algoritmo_cambio.cpp
GUI_APP := progvoraz_gui
GUI_SRC_WIN := gui_window.cpp moneda_gestion.cpp bigint.cpp algoritmo_cambio.cpp vector_dinamico.cpp
GUI_SRC_MAC := gui_macos.swift
GUI_SRC_PORTABLE := gui_portable.cpp moneda_gestion.cpp bigint.cpp algoritmo_cambio.cpp vector_dinamico.cpp

CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic
LDFLAGS ?=
ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

BIN_EXT :=
ifeq ($(OS),Windows_NT)
BIN_EXT := .exe
endif

UNAME_S := $(shell uname -s 2>/dev/null)

TARGET := $(APP)$(BIN_EXT)

.PHONY: all debug release run gui run-gui test clean help

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


test: $(TARGET)
	$(CXX) $(CXXFLAGS) -I. tests/test_bigint_algoritmo.cpp algoritmo_cambio.cpp bigint.cpp vector_dinamico.cpp -o progvoraz_tests
	./progvoraz_tests
	$(CXX) $(CXXFLAGS) -I. tests/test_stock_tx.cpp moneda_gestion.cpp bigint.cpp vector_dinamico.cpp -o progvoraz_stock_tests
	./progvoraz_stock_tests
	./$(TARGET) --help
	./$(TARGET) --input tests/sample_batch.csv --output tests/batch_out.csv
	grep 'OK total=' tests/batch_out.csv
	./$(TARGET) --export-stock-json tests/stock_out.json
	./$(TARGET) --export-report tests/report_out.txt
	./$(TARGET) --convert EUR USD 100 | grep 'Destino=109'

clean:
	rm -f progvoraz progvoraz.exe progvoraz_gui progvoraz_gui.exe temporal.exe stock.tmp progvoraz_tests progvoraz_stock_tests tests/batch_out.csv tests/stock_out.json tests/report_out.txt *.o

help:
	@echo "Targets disponibles:"
	@echo "  make debug    -> compila con simbolos de depuracion"
	@echo "  make release  -> compila optimizado"
	@echo "  make run      -> ejecuta el programa"
	@echo "  make gui      -> compila interfaz GUI (Windows nativa / macOS/Linux portable)"
	@echo "  make run-gui  -> ejecuta interfaz GUI"
	@echo "  make clean    -> elimina artefactos"
