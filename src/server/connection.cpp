#include <sstream>

#include "connection.h"
#include "hook.h"
#include "provider.h"
#include "test.pb.h"

static bool debug = false;

connection::connection(int fd_sock) : m_fd_sock(fd_sock) {
    static int id = 0;
    m_id = id++;
    m_state = state::RECV_SIZE;

    m_send_buf.reserve(1024);
    m_send_off = 0;
    m_to_send = 0;

    m_recv_buf.reserve(1024);
    m_recv_off = 0;
    m_to_recv = sizeof(uint32_t);
    m_recv_buf.resize(m_to_recv);
}

void connection::run() {
    test::RpcHeader header;
    google::protobuf::Message *req = nullptr;
    google::protobuf::Message *resp = nullptr;
    google::protobuf::Service *service = nullptr;
    const google::protobuf::ServiceDescriptor *service_desc = nullptr;
    const google::protobuf::MethodDescriptor *method_desc = nullptr;
    while (true) {
        if (m_state == state::RECV_SIZE) {
            if (debug) std::cout << "s.state: RECV_SIZE" << "\n";
            while (m_recv_off < m_to_recv) {
                int r = recv(m_fd_sock, m_recv_buf.data() + m_recv_off, m_to_recv - m_recv_off, 0);
                m_recv_off += r; // 错误处理 & 协程 yield 在 hook 中完成
            }
            m_to_recv = *(uint32_t *)m_recv_buf.data();
            m_recv_off = 0;
            m_recv_buf.resize(m_to_recv);
            m_state = state::RECV_HDR;
        }

        if (m_state == state::RECV_HDR) {
            if (debug) std::cout << "s.state: RECV_HDR" << "\n";
            while (m_recv_off < m_to_recv) {
                int r = recv(m_fd_sock, m_recv_buf.data() + m_recv_off, m_to_recv - m_recv_off, 0);
                m_recv_off += r;
            }
            header.ParseFromString(m_recv_buf);
            m_recv_off = 0;
            m_to_recv = header.pkg_size();
            m_recv_buf.resize(m_to_recv);
            m_state = state::RECV_BODY;
            service = provider::instance().get_service(header.service()); // 单例模式访问 provider
            service_desc = service->GetDescriptor();
            method_desc = service_desc->FindMethodByName(header.method());
            req = service->GetRequestPrototype(method_desc).New();
            resp = service->GetResponsePrototype(method_desc).New();
        }

        if (m_state == state::RECV_BODY) {
            if (debug) std::cout << "s.state: RECV_BODY" << "\n";
            while (m_recv_off < m_to_recv) {
                int r = recv(m_fd_sock, m_recv_buf.data() + m_recv_off, m_to_recv - m_recv_off, 0);
                m_recv_off += r;
            }
            req->ParseFromString(m_recv_buf);
            service->CallMethod(method_desc, nullptr, req, resp, nullptr);

            std::string resp_str = resp->SerializeAsString();
            m_to_send = sizeof(uint32_t) + resp_str.size();
            std::stringstream message_stream; uint32_t size = resp_str.size();
            message_stream.write((char *)&size, sizeof(size));
            message_stream << resp_str;
            m_send_buf = message_stream.str();
            
            m_send_off = 0;
            m_state = state::SEND_RESP;
        }

        if (m_state == state::SEND_RESP) {
            if (debug) std::cout << "s.state: SEND_RESP" << "\n";
            while (m_send_off < m_to_send) {
                int r = send(m_fd_sock, m_send_buf.data() + m_send_off, m_to_send - m_send_off, 0);
                m_send_off += r;
            }
            // 回到初始状态，准备接收这个连接上的下一个请求。
            m_recv_buf.resize(sizeof(uint32_t));
            m_to_recv = sizeof(uint32_t);
            m_recv_off = 0;
            m_state = state::RECV_SIZE;
        }
    }
}