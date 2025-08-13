#ifndef PROVIDER_H
#define PROVIDER_H

#include <map>
#include <string>
#include "test.pb.h"

class provider {
public:
    provider();

    // 注册服务
    void register_service(const std::string& service_name, google::protobuf::Service* service) {
        m_services[service_name] = service;
    }
    google::protobuf::Service* get_service(const std::string& service_name) {
        auto it = m_services.find(service_name);
        if (it != m_services.end()) {
            return it->second;
        }
        return nullptr;
    }

    void run(int sock);

private:
    std::map<std::string, google::protobuf::Service*> m_services;
};

#endif