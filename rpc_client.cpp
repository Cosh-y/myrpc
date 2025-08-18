#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include "test.pb.h"

class channel : public google::protobuf::RpcChannel {
public:
    channel(const std::string& server_ip, uint16_t port) : m_server_ip(server_ip), m_port(port) {}

    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController* controller,
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done) override {
        // 统一为所有 rpc method 发送请求，接收响应
        const google::protobuf::ServiceDescriptor *sd = method->service();
        std::string service_name = sd->name();
        std::string method_name = method->name();
        std::string request_data = request->SerializeAsString();

        test::RpcHeader header;
        header.set_service(service_name);
        header.set_method(method_name);
        header.set_pkg_size(request_data.size());

        std::string header_str = header.SerializeAsString();
        
        // 使用 stringstream 构造消息，前 4 个字节为 header 长度
        std::stringstream message_stream;
        
        // 写入 header 长度（4 字节，小端序）
        uint32_t header_size = header_str.size();
        std::cout << "header_size: " << header_size << std::endl;
        message_stream.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
        message_stream.write(header_str.data(), header_str.size());
        message_stream.write(request_data.data(), request_data.size());
        
        std::string message_str = message_stream.str();

        sockaddr_in sockaddr;
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = htons(m_port);
        inet_pton(AF_INET, m_server_ip.c_str(), &sockaddr.sin_addr);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        connect(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr));

        // 发送请求
        std::cout << "Sending message of size: " << message_str.size() << " bytes" << std::endl;
        std::cout << "Sending message: " << message_str << std::endl;
        send(sock, message_str.data(), message_str.size(), 0);

        // 接收响应长度（4 字节）
        uint32_t response_size;
        recv(sock, &response_size, sizeof(response_size), 0);
        
        // 接收响应数据
        std::string response_data(response_size, 0);
        recv(sock, response_data.data(), response_size, 0);
        
        // 解析响应
        response->ParseFromString(response_data);

        close(sock);
    }

private:
    std::string m_server_ip;
    uint16_t m_port;
};

// 上面的 channel 为 RPC 框架要提供的
// 下面的 main 是用户要写的

int main() {
    test::EchoService_Stub stub(new channel("localhost", 8080));
    google::protobuf::RpcController* controller = nullptr;
    google::protobuf::Closure* done = nullptr;
    
    test::EchoRequest request;
    test::EchoResponse response;
    request.set_id(1);
    request.set_content("Hello, RPC!");

    stub.Echo(controller, &request, &response, done);

    uint32_t id = response.id();
    std::string res = response.content();
    std::cout << "Response ID: " << id << ", Content: " << res << std::endl;
    return 0;
}