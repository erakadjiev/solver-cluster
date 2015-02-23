/*
 * client_local.cpp
 *
 *  Created on: Feb 10, 2015
 *      Author: rakadjiev
 */


#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

std::string exec_solver(const std::string query){
	fflush(stdout);
	fflush(stderr);

	int p2cPipe[2];
	int c2pPipe[2];

	if(
			pipe(p2cPipe) ||
			pipe(c2pPipe)){
		std::cerr << "Failed to pipe\n";
		return "";
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
		std::string ans;

		while((readBytes = read(c2pPipe[0], buf, sizeof(buf)-1)) > 0){
			ans.append(buf, readBytes);
		}
		waitpid(pid, NULL, 0);

		close(c2pPipe[0]);
		return ans;
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
		return "";
	}
}

int main(int argc, char* argv[]){
	std::string smt_query(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());

	std::cout << "client1 starting...\n";

    for(int i = 0; i<10000; ++i){
    	std::string ans = exec_solver(smt_query);
    }

	std::cout << "client1 exiting...\n";
	return 0;
}

