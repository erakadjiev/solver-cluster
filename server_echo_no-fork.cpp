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
#include <fstream>
#include <unistd.h>

const std::string own_port = "6790";
const std::string join_ip = "10.232.107.213";
zhashx_t* member_set;

zsock_t* solver_service;

static const std::string get_own_ip();
void reset_member_set();
void parse_hosts_file(std::string hosts_path);
void process_membership_event(const std::string event_type, const std::string member_ip, const std::string port);
int membership_handler(zloop_t* reactor, zsock_t* membership_socket, void* arg);
int exec_serf(const std::string serf_path);
int discovery_handler(zloop_t* reactor, zsock_t* discovery, void *arg);
int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg);

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

void reset_member_set(){
	if(member_set != NULL){
		zhashx_destroy(&member_set);
	}
	member_set = zhashx_new();
	zhashx_insert(member_set, (own_ip + ":" + own_port).c_str(), (void*)"dummy");
}

void parse_hosts_file(std::string hosts_path){
	reset_member_set();
	std::ifstream hosts_file(hosts_path.c_str());
	std::string host;
	while(std::getline(hosts_file, host)){
		zhashx_insert(member_set, host.c_str(), (void*)"dummy");
	}
}

void process_membership_event(const std::string event_type, const std::string member_ip, const std::string port){
	// make global
	const std::string join_type = "member-join";
	const std::string leave_type = "member-leave";
	const std::string fail_type = "member-failed";

	std::string member_addr = member_ip + ':' + (port.empty() ? "6790" : port);

	if (event_type == join_type){
		zhashx_insert(member_set, member_addr.c_str(), (void*)"dummy");
	} else if ((event_type == leave_type) || (event_type == fail_type)) {
		if(zhashx_lookup(member_set, member_addr.c_str()) != NULL){
			zhashx_delete(member_set, member_addr.c_str());
		} else {
			std::cerr << member_addr << " left the cluster, but we didn't know him.\n";
		}
	} else {
		std::cerr << "Received unknown membership event: " << event_type << " for " << member_addr << "\n";
	}
}

int membership_handler(zloop_t* reactor, zsock_t* membership_socket, void* arg){
	char* event_type;
	char* ip;
	char* port;
	int count = zstr_recvx(membership_socket, &event_type, &ip, &port, NULL);
	if(count != 3){
		std::cerr << count << "-part message received. Membership handler expects 3-part messages.\n";
		return -1;
	}

	process_membership_event(event_type, ip, port);

////If we receive non-formatted batch of logs:
//	std::regex r("([a-z\\-]+)\\t([0-9\\.]+)");
//	std::sregex_iterator it(msg.begin(), msg.end(), r);
//	std::sregex_iterator it_end;
//	while(it != it_end){
//		if ((*it).size() != 3){
//			std::cerr << "Regex matching problem.\n";
//		} else {
//			process_membership_event((*it)[1].str(), (*it)[2].str());
//		}
//		++it;
//	}

	zstr_free(&event_type);
	zstr_free(&ip);
	return 0;
}

int exec_serf(const std::string serf_path){
	fflush(stdout);
	fflush(stderr);

	pid_t pid = fork();
	if (pid > 0) {
		// parent
		// TODO set up termination detection for child process
		reset_member_set();

		return pid;
	}
	else if (pid == 0) {
		// child

		// we don't want to see Serf output
		// TODO log to file instead
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		char* tmp = new char[4096];
		std::string serf_abs_path = realpath(serf_path.c_str(), tmp);
		delete[] tmp;

		int ret = execl(serf_abs_path.c_str(), serf_abs_path.c_str(), "agent",  "-event-handler=/home/ltc/workspace/serf-handler", ("-join=" + join_ip).c_str(), "-tag", ("port=" + own_port).c_str(), (char*)NULL);

		exit(ret);
	}
	else {
		std::cerr << "Failed to fork serf\n";
		return -1;
	}
}

int discovery_handler(zloop_t* reactor, zsock_t* discovery, void *arg){
	zmsg_t* req = zmsg_recv(discovery);
	zmsg_destroy(&req);
	std::cout << "Received discovery request\n";
	std::cout << "Cluster size: " << zhashx_size(member_set) << "\n";
	zframe_t* frame = zhashx_pack(member_set);
	zframe_send(&frame, discovery, 0);
	return 0;
}

int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg){
	zmsg_t* msg = zmsg_recv(solver_service);
	zframe_t* identity = zmsg_pop(msg);
	std::string id = zmsg_popstr(msg);
	std::string que = zmsg_popstr(msg);
	zmsg_destroy(&msg);
	std::cout << "Received: " << que << "\n";

	zmsg_t* ans_msg = zmsg_new();
	zmsg_addstr(ans_msg, id.c_str());
	zmsg_addstr(ans_msg, que.c_str());
	zmsg_prepend(ans_msg, &identity);
	zmsg_send(&ans_msg, solver_service);

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
	solver_service = zsock_new_router("tcp://*:6790");
	assert(solver_service);

	zsock_t* membership_socket = zsock_new_pull("ipc:///tmp/testsock");
	assert(membership_socket);

	bool dynamic_membership = true;
	int serf_pid;
	if(dynamic_membership){
		serf_pid = exec_serf("/home/ltc/workspace/serf/serf");
	} else {
		parse_hosts_file("hosts.txt");
	}

	zloop_t* reactor = zloop_new();
	zloop_reader(reactor, discovery, discovery_handler, NULL);
	zloop_reader(reactor, solver_service, solver_handler, NULL);
	zloop_reader(reactor, membership_socket, membership_handler, NULL);
	zloop_start(reactor);

	zloop_destroy(&reactor);
	zsock_destroy(&solver_service);
	zsock_destroy(&discovery);
	zsock_destroy(&membership_socket);
	zhashx_destroy(&member_set);
	waitpid(serf_pid, NULL, WNOHANG | WUNTRACED);
	std::cout << "Server exiting...\n";
	return 0;
}


