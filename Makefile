SOURCE_DIR := src
TARGET_DIR := build
SOURCES    := $(SOURCE_DIR)/imgedit.cpp
SOURCES    += $(SOURCE_DIR)/main.cpp
HEADERS    := $(SOURCE_DIR)/imgedit.h
TARGET     := $(TARGET_DIR)/imgedit

CXXFLAGS := -O3 -std=c++11 -Wall -Werror
CXX = g++ $(CXXFLAGS)

all: $(TARGET)

clean:
	-rm $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	-mkdir $(TARGET_DIR)
	$(CXX) $(SOURCES) -o $(TARGET)
