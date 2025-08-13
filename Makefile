# Makefile for myrpc project

CXX := g++
# 基础编译选项
BASE_CXXFLAGS := -std=c++17 -Wall -Isrc/server/include -Isrc/protobuf `pkg-config --cflags protobuf`
# 发布版本编译选项
RELEASE_CXXFLAGS := $(BASE_CXXFLAGS) -O2 -DNDEBUG
# 调试版本编译选项
DEBUG_CXXFLAGS := $(BASE_CXXFLAGS) -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
# 默认使用发布版本
CXXFLAGS := $(RELEASE_CXXFLAGS)
LDFLAGS := `pkg-config --libs protobuf`
SERVER_DIR := src/server
CLIENT_DIR := src/client
PROTO_DIR := src/protobuf
BIN_DIR := bin
CBIN_DIR := src/client
STARGET := $(BIN_DIR)/myrpc_server
CTARGET := $(BIN_DIR)/myrpc_client

# 获取所有源文件
SERVER_SRCS := $(wildcard $(SERVER_DIR)/*.cpp)
CLIENT_SRCS := $(wildcard $(CLIENT_DIR)/*.cpp)
PROTO_SRCS := $(wildcard $(PROTO_DIR)/*.cc)
ALL_SRCS := $(SERVER_SRCS) $(PROTO_SRCS)

# 生成目标文件路径
SERVER_OBJS := $(SERVER_SRCS:$(SERVER_DIR)/%.cpp=$(BIN_DIR)/%.o)
CLIENT_OBJS := $(CLIENT_SRCS:$(CLIENT_DIR)/%.cpp=$(CBIN_DIR)/%.o)
PROTO_OBJS := $(PROTO_SRCS:$(PROTO_DIR)/%.cc=$(BIN_DIR)/%.o)
S_ALL_OBJS := $(SERVER_OBJS) $(PROTO_OBJS)
C_ALL_OBJS := $(CLIENT_OBJS) $(PROTO_OBJS)

# 编译规则：server目录下的 .cpp 文件
$(BIN_DIR)/%.o: $(SERVER_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/%.o: $(CLIENT_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 编译规则：protobuf目录下的 .cc 文件
$(BIN_DIR)/%.o: $(PROTO_DIR)/%.cc
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: $(STARGET) $(CTARGET)

# 调试版本目标
debug: CXXFLAGS := $(DEBUG_CXXFLAGS)
debug: LDFLAGS += -fsanitize=address -fsanitize=undefined
debug: clean $(STARGET) $(CTARGET)

# 发布版本目标
release: CXXFLAGS := $(RELEASE_CXXFLAGS)
release: clean $(STARGET) $(CTARGET)

$(STARGET): $(S_ALL_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(CTARGET): $(C_ALL_OBJS)
	$(CXX) -std=c++17 -Wall -Isrc/protobuf `pkg-config --cflags protobuf` $^ -o $@ $(LDFLAGS)

.PHONY: clean debug release gdb valgrind
clean:
	rm -rf $(BIN_DIR)/*.o $(STARGET) $(CTARGET)

# 使用 gdb 运行服务器
gdb: debug
	gdb $(STARGET)

# 使用 valgrind 检查内存泄漏
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(STARGET)

# 显示编译选项
show-flags:
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"