/*
 * client.cpp
 *
 *  Created on: Jan 22, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>
#include <iostream>

int main(int argc, char* argv[]){
	std::string smt_query(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());

	zsock_t* discovery = zsock_new_req("tcp://localhost:6789");
	assert(discovery);


	std::cout << "client1 starting...\n";
	int rc = zstr_send(discovery, "DISC");
	assert(rc == 0);
	std::cout << "Sent discovery request\n";
	std::string rep = zstr_recv(discovery);
	std::cout << "Service endpoint(s) discovered:\n" + rep + "\n";

	zsock_t* service = zsock_new_dealer(rep.c_str());
	assert(service);
	std::cout << "Sending SMT query to service\n";
	rc = zstr_send(service, smt_query.c_str());
	assert(rc == 0);
	std::string ans = zstr_recv(service);
	std::cout << ans + "\n";

	zsock_destroy(&discovery);
	zsock_destroy(&service);
	std::cout << "client1 exiting...\n";
	return 0;
}
