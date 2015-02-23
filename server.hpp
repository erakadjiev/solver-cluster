/*
 * solver.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rakadjiev
 */

#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <czmq.h>
#include <string>

struct solver_reply_info{
public:
	explicit solver_reply_info(zframe_t* _identity, int _pid, zsock_t* _solver_service, std::string _message_id) :
		identity(_identity), pid(_pid), solver_service(_solver_service), message_id(_message_id){}
	zframe_t* identity;
	int pid;
	zsock_t* solver_service;
	std::string message_id;
};

struct solver_proc_info{
public:
	explicit solver_proc_info(int _pid, int _fd) :
		pid(_pid), fd(_fd){}
	int pid;
	int fd;
};

static const std::string get_own_ip();
void reset_member_set();
void parse_hosts_file(std::string hosts_path);
void process_membership_event(const std::string event_type, const std::string member_ip, const std::string port);
int membership_handler(zloop_t* reactor, zsock_t* membership_socket, void* arg);
int exec_serf(const std::string serf_path);
solver_proc_info* exec_solver(const std::string query);
int solver_result_handler(zloop_t* reactor, zmq_pollitem_t* child_pipe, void* arg);
int discovery_handler(zloop_t* reactor, zsock_t* discovery, void *arg);
int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg);


#endif /* SERVER_HPP_ */
