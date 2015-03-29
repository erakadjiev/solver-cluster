/*
 * client_new.cpp
 *
 *  Created on: Mar 23, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>

#include <string>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <iostream>

#include "suppressive_round_robin.hpp"
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/future.hpp>
#include <boost/fiber/segmented_stack.hpp>
#include <boost/fiber/operations.hpp>

zsock_t* discovery;
zsock_t* service;
zpoller_t* service_poller;
zhashx_t* solvers;

class SolverReply{
public:
	SolverReply() : status(0), reply(nullptr){}
	SolverReply(int status, char* reply) : status(0), reply(reply){}
	bool isEmpty(){
		return (!status && !reply);
	}
	int status;
	char* reply;
};

std::unordered_map<unsigned int, boost::fibers::promise<SolverReply>> pendingQueries;

unsigned int reqId;
const unsigned int maxId = UINT_MAX-1;

void find_and_set_solver_backends(){
	int rc = zstr_send(discovery, "DISC");
	assert(rc == 0);
	//    std::cout << "Sent discovery request\n";
	zframe_t* rep = zframe_recv(discovery);
	solvers = zhashx_unpack(rep);
	zframe_destroy(&rep);
	//    std::cout << "Service endpoint(s) discovered: {\n";
	for(void* solver = zhashx_first(solvers); solver != NULL; solver = zhashx_next(solvers)){
		std::string solver_addr = (const char*)zhashx_cursor(solvers);
		zsock_connect(service, "%s%s", "tcp://", solver_addr.c_str());
		//      std::cout << '\t' << solver_addr << "\n";
	}
	//    std::cout << "}\n";
}

std::string read_smt_file(std::string path){
	std::ifstream smt_file(path.c_str());
	std::string host;
	std::string contents((std::istreambuf_iterator<char>(smt_file)), std::istreambuf_iterator<char>());
	return contents;
}

int generate_request_id(){
	if(++reqId > maxId){
		reqId = 0;
	}
	return reqId;
}

SolverReply send_query(std::string query){
  int req_id = generate_request_id();
  zstr_sendm(service, std::to_string(req_id).c_str());
  int rc = zstr_send(service, query.c_str());
  assert(rc == 0);
  
  bool gotOwnResponse = false;
  SolverReply solver_reply;
  while (zsock_events(service) & ZMQ_POLLIN) {
    zmsg_t* msg = zmsg_recv(service);
    char* response_id = zmsg_popstr(msg);
    char* response_status = zmsg_popstr(msg);

    int resp_id = std::stoi(response_id);
    int resp_status = std::stoi(response_status);

    char* response = nullptr;
    if(resp_status == 0){
      response = zmsg_popstr(msg);
    }
    solver_reply = SolverReply(resp_status, response);

    zstr_free(&response_id);
    zstr_free(&response_status);
    zmsg_destroy(&msg);
    
    if(req_id == resp_id){
      gotOwnResponse = true;
      std::cout << "Got own response immediately\n";
    } else {
      auto elem = pendingQueries.find(resp_id);
      
      if (elem == pendingQueries.end()){
        std::cout << "Promise not found\n";
      } else {
        elem->second.set_value(solver_reply);
      }
    }
  }

  if(!gotOwnResponse){
    boost::fibers::promise<SolverReply> p;
    boost::fibers::future<SolverReply> f = p.get_future();
    pendingQueries.insert(std::make_pair(req_id, std::move(p)));

    solver_reply = f.get();
    pendingQueries.erase(req_id);
  }
  
  if (solver_reply.isEmpty()){
    std::cerr << "Received \"null\" response from server\n";
  }
  return solver_reply;
}

void wait_for_response(){
  while (!(zsock_events(service) & ZMQ_POLLIN)){
    void* ret = zpoller_wait(service_poller, 500);
    if(ret == NULL){
      if(zpoller_terminated(service_poller)){
        std::cout << "Service poller terminated\n";
      } else {
        std::cout << "Service poller failed (unknown reason)\n";
      }
      return;
    }
  }
  while (zsock_events(service) & ZMQ_POLLIN) {
    zmsg_t* msg = zmsg_recv(service);
    char* response_id = zmsg_popstr(msg);
    char* response_status = zmsg_popstr(msg);

    int resp_id = std::stoi(response_id);
    int resp_status = std::stoi(response_status);

    char* response = nullptr;
    if(resp_status == 0){
      response = zmsg_popstr(msg);
    }
    SolverReply solver_reply(resp_status, response);

    zstr_free(&response_id);
    zstr_free(&response_status);
    zmsg_destroy(&msg);
    
    auto elem = pendingQueries.find(resp_id);
    
    if (elem == pendingQueries.end()){
      std::cout << "Promise not found\n";
    } else {
      elem->second.set_value(solver_reply);
    }
  }
}

void solve(std::string& smtLibQuery) {
	SolverReply queryResult = send_query(smtLibQuery);
	int status = queryResult.status; 
	if(std::strncmp("sat", queryResult.reply, 3) != 0){
		std::cout << "ERROR: invalid reply: " << queryResult.reply << "\n";
	}
	zstr_free(&queryResult.reply);
}

int main(int argc, char* argv[]){
	if(argc < 2){
		std::cout << "Usage: " << argv[0] << " num_times [path_to_query]\n"; 
		return 1;
	}
	
	discovery = zsock_new_req("tcp://172.31.0.6:6789");
	service = zsock_new(ZMQ_DEALER);
	service_poller = zpoller_new(service, NULL);
	
	find_and_set_solver_backends();
//	zsock_connect(service, "ipc:///tmp/solver_service");
	
	int times = std::stoi(argv[1]);
	std::string smt_path;
	if(argc > 2){
		smt_path = argv[2];
	} else {
		smt_path = "query.smt2";
	}

	int max_fibers = 1000;
	int running_fibers = 0;
	int solved = 0;

	boost::fibers::suppressive_round_robin scheduler;
	boost::fibers::set_scheduling_algorithm(&scheduler);
	boost::fibers::fiber runFiber(std::allocator_arg, boost::fibers::segmented_stack(), [&] () {
		while((times > 0) || (running_fibers > 0)){
			if((running_fibers <= max_fibers) && (times > 0)){
				boost::fibers::fiber(std::allocator_arg, boost::fibers::segmented_stack(), [&] () {
					++running_fibers;
					while(times > 0){
						--times;
						std::string query = read_smt_file(smt_path);
						solve(query);
						++solved;
						if(solved % 5000 == 0){
							std::cout << solved << " solved\n";
						}
						
					}
					--running_fibers;
				} ).detach();
			} else {
				//      std::cout << "Searcher empty\n";
				assert(running_fibers > 0);
				wait_for_response();
			}
			boost::this_fiber::yield();
		}
	} );
	scheduler.set_suppressed_fiber(runFiber.get_id());
	runFiber.join();
	
//	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
//	zsock_destroy(&discovery);
	zpoller_destroy(&service_poller);
	zsock_destroy(&service);
	return 0;
}
