CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3

TARGET = bpt_driver
SRC = bplustree.cpp driver.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean


