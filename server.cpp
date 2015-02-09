/*
 * server.cpp
 *
 *  Created on: Jan 22, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>
#include <string>
#include <cstring>
#include <iostream>

const std::string own_port = "6790";

struct solver_reply_info{
public:
	explicit solver_reply_info(zframe_t* _identity, zsock_t* _solver_service) :
		identity(_identity), solver_service(_solver_service){}
	zframe_t* identity;
	zsock_t* solver_service;
};

static const std::string get_own_ip(){
	std::string ip = "";

	struct ifaddrs* if_addrs;
	int ret = getifaddrs(&if_addrs);

	if(ret == 0){
		for (struct ifaddrs* if_addr = if_addrs; if_addr != NULL; if_addr = if_addr->ifa_next) {
			if (if_addr->ifa_addr->sa_family == AF_INET) {
				if(std::strcmp("eth0", if_addr->ifa_name) == 0){
					void* net_addr = &((struct sockaddr_in*)if_addr->ifa_addr)->sin_addr;
					char tmp[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, net_addr, tmp, INET_ADDRSTRLEN);
					ip = tmp;
				}
			}
		}
		if(if_addrs != NULL){
			freeifaddrs(if_addrs);
		}
	}

	return ip;
}

const std::string own_ip = get_own_ip();

int execSolver(const std::string query){
	fflush(stdout);
	fflush(stderr);

	int p2cPipe[2];
	int c2pPipe[2];

	if(
			pipe(p2cPipe) ||
			pipe(c2pPipe)){
		std::cerr << "Failed to pipe\n";
		return -1;
	}

	pid_t pid = fork();
	if (pid > 0) {
		// parent
		close(p2cPipe[0]);
		close(c2pPipe[1]);

		int ret = write(p2cPipe[1], query.c_str(), query.length()+1);
		close(p2cPipe[1]);

		// TODO return child pid, so that we can wait on it later (to avoid having zombies)
		return c2pPipe[0];
	}
	else if (pid == 0) {
		// child
		if(
				dup2(p2cPipe[0], STDIN_FILENO) == -1 ||
				dup2(c2pPipe[1], STDOUT_FILENO) == -1 ||
				dup2(c2pPipe[1], STDERR_FILENO) == -1){
			std::cerr << "Failed to redirect stdin/stdout/stderr\n";
			exit(1);
		}
		close(p2cPipe[0]);
		close(p2cPipe[1]);
		close(c2pPipe[0]);
		close(c2pPipe[1]);

		int ret = execl("/home/ltc/workspace/stpwrap2/build/stpwrap2", "/home/ltc/workspace/stpwrap2/build/stpwrap2", (char*)NULL);
		exit(ret);
	}
	else {
		std::cerr << "Failed to fork!\n";
		return -1;
	}
}

int solver_result_handler(zloop_t* reactor, zmq_pollitem_t* child_pipe, void* arg){
	// TODO get child pid and wait on it (to avoid having zombies)
	int fd = child_pipe->fd;
	int readBytes = 1;
	char buf[100];
	std::string ans;

	while((readBytes = read(fd, buf, sizeof(buf)-1)) > 0){
		ans.append(buf, readBytes);
	}

	zloop_poller_end(reactor, child_pipe);
	close(fd);

	solver_reply_info* rep = (solver_reply_info*)arg;
	zmsg_t* ans_msg = zmsg_new();
	zmsg_addstr(ans_msg, ans.c_str());
	zmsg_prepend(ans_msg, &(rep->identity));
	zmsg_send(&ans_msg, rep->solver_service);
	std::cout << "Sent solver_service answer\n";
	delete rep;

	return 0;
}

int discovery_handler(zloop_t* reactor, zsock_t* discovery, void *arg){
	zmsg_t* req = zmsg_recv(discovery);
	zmsg_destroy(&req);
	std::cout << "Received discovery request\n";
	zstr_send(discovery, (own_ip + ":" + own_port).c_str());
	return 0;
}

int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg){
	zmsg_t* msg = zmsg_recv(solver_service);
	zframe_t* identity = zmsg_pop(msg);
	std::string que = zmsg_popstr(msg);
	zmsg_destroy(&msg);
	std::cout << "Received solver_service query\n";

	int fd = execSolver(que);

	zmq_pollitem_t child_pipe;
	child_pipe.socket = NULL;
	child_pipe.fd = fd;
	child_pipe.events = ZMQ_POLLIN;

	solver_reply_info* rep = new solver_reply_info(identity, solver_service);

	zloop_poller(reactor, &child_pipe, solver_result_handler, rep);

	return 0;
}

int main(int argc, char* argv[]){
//	if(argc < 2){
//		return 1;
//	}
//	std::string port = argv[1];


	std::cout << "Starting discovery service on port 6789...\n";
	zsock_t* discovery = zsock_new_rep("tcp://*:6789");
	assert(discovery);

	std::cout << "Starting solver service on port 6790...\n";
	zsock_t* solver_service = zsock_new_router("tcp://*:6790");
	assert(solver_service);

	zloop_t* reactor = zloop_new();
	zloop_reader(reactor, discovery, discovery_handler, NULL);
	zloop_reader(reactor, solver_service, solver_handler, NULL);
	zloop_start(reactor);

	zloop_destroy(&reactor);
	zsock_destroy(&solver_service);
	zsock_destroy(&discovery);
	std::cout << "Server exiting...\n";
	return 0;
}


