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
