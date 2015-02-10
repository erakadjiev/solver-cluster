/*
 * solver.h
 *
 *  Created on: Feb 10, 2015
 *      Author: rakadjiev
 */

#ifndef SERVER_H_
#define SERVER_H_

#include <czmq.h>
#include <string>

static const std::string get_own_ip();
void reset_member_set();
void parse_hosts_file(std::string hosts_path);
void process_membership_event(const std::string event_type, const std::string member_ip, const std::string port);
int membership_handler(zloop_t* reactor, zsock_t* membership_socket, void* arg);
int exec_serf(const std::string serf_path);
int execSolver(const std::string query);
int solver_result_handler(zloop_t* reactor, zmq_pollitem_t* child_pipe, void* arg);
int discovery_handler(zloop_t* reactor, zsock_t* discovery, void *arg);
int solver_handler(zloop_t* reactor, zsock_t* solver_service, void *arg);


#endif /* SERVER_H_ */
