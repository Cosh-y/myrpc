#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include "test.pb.h"
#include "hook.h"
#include "provider.h"

class UserEchoService : public test::EchoService {
    void Echo(google::protobuf::RpcController* controller,
              const test::EchoRequest* request,
              test::EchoResponse* response,
              google::protobuf::Closure* done) override {
        // 这里可以添加用户自定义的逻辑
        response->set_id(request->id());
        response->set_content("UserEchoService: " + request->content());
        
        if (done) {
            done->Run();
        }
    }
};

provider::provider() {
    const google::protobuf::ServiceDescriptor* service_desc = test::EchoService::descriptor();
    register_service(service_desc->name(), new UserEchoService());
}

/// 从 socket 中读请求、反序列化解析、调用注册的服务、序列化返回值、通过 socket 返回响应 
void provider::run(int sock) {
    // 读取请求
    test::RpcHeader header;
    int header_size;
    int r;
    // 对方以这种形式将长度写入前 4 字节
    // message_stream.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
    // 可以把前 4 字节的长度直接读到 int 里，
    if ((r = read(sock, &header_size, sizeof(uint32_t))) != sizeof(uint32_t)) {
        std::cout << "Failed to read RPC header" << std::endl;
        return;
    }

    std::string buffer(header_size, 0);
    if ((r = read(sock, buffer.data(), header_size)) != header_size) {
        std::cout << "Failed to read RPC header data" << std::endl;
        return;
    }

    header.ParseFromString(buffer);
    std::string service_name = header.service();
    std::string method_name = header.method();
    uint32_t pkg_size = header.pkg_size();

    // 调用注册的服务
    google::protobuf::Service* service = get_service(service_name);
    if (!service) {
        std::cout << "Service not found: " << service_name << std::endl;
        return;
    }

    // 获取 method_descriptor
    const google::protobuf::ServiceDescriptor* service_desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method_desc = service_desc->FindMethodByName(method_name);
    if (!method_desc) {
        std::cout << "Method not found: " << method_name << std::endl;
        return;
    }

    // 使用 method_descriptor 获取请求消息的原型
    google::protobuf::Message* request_msg = service->GetRequestPrototype(method_desc).New();
    
    // 读取请求数据
    std::string request_data(pkg_size, 0);
    if (read(sock, request_data.data(), pkg_size) != static_cast<ssize_t>(pkg_size)) {
        std::cout << "Failed to read request data" << std::endl;
        delete request_msg;
        return;
    }
    
    // 反序列化请求数据
    if (!request_msg->ParseFromString(request_data)) {
        std::cout << "Failed to parse request data" << std::endl;
        delete request_msg;
        return;
    }


    google::protobuf::Message* response_msg = service->GetResponsePrototype(method_desc).New();
    google::protobuf::RpcController* controller = nullptr;
    google::protobuf::Closure* done = nullptr;
    service->CallMethod(method_desc, controller, request_msg, response_msg, done);

    std::string response_data;
    if (!response_msg->SerializeToString(&response_data)) {
        std::cout << "Failed to serialize response" << std::endl;
        delete request_msg;
        delete response_msg;
        return;
    }

    // 发送响应数据，开头四字节依旧为数据长度
    uint32_t response_size = response_data.size();
    std::stringstream response_stream;
    response_stream.write(reinterpret_cast<const char*>(&response_size), sizeof(response_size));
    response_stream << response_data;
    if (write(sock, response_stream.str().data(), response_stream.str().size()) != static_cast<ssize_t>(response_stream.str().size())) {
        std::cout << "Failed to send response data" << std::endl;
        delete request_msg;
        delete response_msg;
        return;
    }

    // 清理资源
    delete request_msg;
    delete response_msg;
}
