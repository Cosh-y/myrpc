# Makefile for myrpc project

# 项目结构假定如下：
# /home/harry/Documents/myrpc/
# ├── src/
# │   └── *.cpp
# ├── include/
# │   └── *.h
# └── bin/

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Isrc/include
SRC_DIR := src
BIN_DIR := bin
TARGET := $(BIN_DIR)/myrpc

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BIN_DIR)/%.o) # 模式替换语法 $(变量名：模式1=模式2)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -rf $(BIN_DIR)/*.o $(TARGET)