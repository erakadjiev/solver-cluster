/*
 * server_local.h
 *
 *  Created on: Mar 22, 2015
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

solver_proc_info* exec_solver(const std::string query);
int solver_result_handler(zloop_t* reactor, zmq_pollitem_t* child_pipe, void* arg);
int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg);


#endif /* SERVER_HPP_ */
