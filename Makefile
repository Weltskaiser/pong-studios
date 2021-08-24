CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I src

#SANITIZE = true

ifdef SANITIZE
CXXFLAGS += -g -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
endif

SRC = $(wildcard ./src/*.cpp) $(wildcard ./src/*/*.cpp)
OBJ = $(SRC:.cpp=.o)
LIB = -lmkvwriter -lebml -lmatroska -lFLAC -lx264 -lsfml-graphics -lsfml-system -lsfml-window

TARGET = STD.exe

all: $(TARGET)
clean:
	rm -f $(OBJ) $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) $(LIB) -o $(TARGET)

.PHONY: all clean