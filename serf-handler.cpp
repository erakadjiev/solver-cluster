/*
 * serf-handler.cpp
 *
 *  Created on: Feb 9, 2015
 *      Author: rakadjiev
 */

#include <iostream>
#include <czmq.h>

const std::string port_tag = "port=";

std::string strip_port_tag(std::string s){
	std::string::size_type ret = s.rfind(port_tag);
	if(ret == std::string::npos){
		return "";
	} else {
		std::string port = s.substr(ret + port_tag.length());
		return port;
	}
}

int main(int argc, char** argv) {
	zsock_t* push = zsock_new_push("ipc:///tmp/testsock");
	zsock_set_linger (push, -1);

	std::string line;
	std::string ip;
	std::string port;
	const char* event_type = getenv("SERF_EVENT");
	while (std::getline(std::cin, line, '\t')) {
		std::getline(std::cin, line, '\t');
		ip = line;
		// jump over deprecated role field
		std::getline(std::cin, line, '\t');
		// tag is the last part of the event, so read until the end of the line
		std::getline(std::cin, line);
		port = strip_port_tag(line);
		zstr_sendx(push, event_type, ip.c_str(), port.c_str(), NULL);
	}

	zsock_destroy(&push);
	return 0;
}
