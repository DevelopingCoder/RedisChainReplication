#include "replicated_server.h"

ReplicatedServer::ReplicatedServer(int port, optional<pair<string, int>> next_server): Server(port) {
    has_prev_server_ = false;
    next_server_fd_ = -1;
    if (next_server) {
        next_server_fd_ = connect_to_server(next_server.value().first, next_server.value().second);
    }
    redis_client_.connect();
}

bool ReplicatedServer::is_tail_server() {
    return next_server_fd_ == -1;
}

void ReplicatedServer::update_has_prev_server(bool has_prev_server) {
    has_prev_server_ = has_prev_server;
}

void ReplicatedServer::update_next_server(pair<string, int> next_server) {
    if (next_server_fd_ > 0) {
        close(next_server_fd_);
    }
    next_server_fd_ = connect_to_server(next_server.first, next_server.second);
}

string
ReplicatedServer::send_redis_cmd(Request request) {
    vector<string> redis_args = {request.cmd(), request.key()};
    if (request.has_val()) {
        redis_args.push_back(request.val());
    }
    string response;
    cout << "Sending redis cmd: " << redis_args[0] << " " << redis_args[1] << endl;
    redis_client_.send(redis_args, [&](cpp_redis::reply& reply) {
        response = reply.as_string();
    });
    redis_client_.sync_commit();
    return response;
}

int
ReplicatedServer::get_client_fd(string host, int port) {
    // Traverse all clients to find the associated fd
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    for (const auto client_fd: client_fds_) {
       getsockname(client_fd, (struct sockaddr *) &client_addr, &client_len);
       string client_host = inet_ntoa(client_addr.sin_addr);
       int client_port = ntohs(client_addr.sin_port);
       if (client_host == host and port == client_port) {
           return client_fd;
       }
    }
    return -1; //Couldn't find associated connection
}

void
ReplicatedServer::handle_request(string request_str) {
    //Convert request to protobuf object
    Request request;
    if (request.ParseFromString(request_str)) {
        string reply = send_redis_cmd(request);
        if (is_tail_server()) {
            int client_fd = get_client_fd(request.client_addr(), request.client_port());
            send_msg(client_fd, reply);
        } else {
            // Send the request to the next server. Make sure to pass the original client's address. Good time to use Protobuf
            send_msg(next_server_fd_, request_str);
        }
    } else {
        cout << "Invalid request format\n";
    }
}

