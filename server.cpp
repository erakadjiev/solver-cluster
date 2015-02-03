/*
 * server.cpp
 *
 *  Created on: Jan 22, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>
#include <string>
#include <iostream>

std::string execSolver(std::string query){
	fflush(stdout);
	fflush(stderr);

	int p2cPipe[2];
	int c2pPipe[2];

	if(
			pipe(p2cPipe) ||
			pipe(c2pPipe)){
		std::cerr << "Failed to pipe\n";
		return "Server error.";
	}

	pid_t pid = fork();
	if (pid > 0) {
		// parent
		close(p2cPipe[0]);
		close(c2pPipe[1]);

		int ret = write(p2cPipe[1], query.c_str(), query.length()+1);
		close(p2cPipe[1]);

		int readBytes = 1;
		char buf[100];
		std::string result;

		while((readBytes = read(c2pPipe[0], buf, sizeof(buf)-1)) > 0){
			result.append(buf, readBytes);
		}

		int status;
		waitpid(pid, &status, 0);
		close(c2pPipe[0]);
		return result;
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

		int ret = execl("/home/rakadjiev/workspace/stpwrap2/build/stpwrap2", "/home/rakadjiev/workspace/stpwrap2/build/stpwrap2", (char*)NULL);
		exit(ret);
	}
	else  {
		std::cerr << "Failed to fork!\n";
		return "Server error.";
	}
}

int main(int argc, char* argv[]){
//	if(argc < 2){
//		return 1;
//	}
//	std::string port = argv[1];
	const std::string server_addr = "tcp://localhost:6790";

	std::cout << "Starting discovery service on port 6789...\n";
	zsock_t* discovery = zsock_new_rep("tcp://*:6789");
	assert(discovery);

	std::cout << "Starting solver service on port 6790...\n";
	zsock_t* service = zsock_new_router("tcp://*:6790");
	assert(service);

	zmsg_t* req = zmsg_recv(discovery);
	std::cout << "Received discovery request\n";
	zstr_send(discovery, server_addr.c_str());

	while(true){
		zmsg_t* msg = zmsg_recv(service);
		zframe_t* identity = zmsg_pop(msg);
		std::string que = zmsg_popstr(msg);
		zmsg_destroy(&msg);
		std::cout << "Received service query\n";
		std::string ans = execSolver(que);
		zmsg_t* ans_msg = zmsg_new();
		zmsg_addstr(ans_msg, ans.c_str());
		zmsg_prepend(ans_msg, &identity);
		zmsg_send(&ans_msg, service);
		std::cout << "Sent service answer\n";
	}

	zsock_destroy(&service);
	zsock_destroy(&discovery);
	std::cout << "Server exiting...\n";
	return 0;
}


