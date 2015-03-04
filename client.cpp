 /*
  * client.cpp
  *
  *  Created on: Jan 22, 2015
  *      Author: rakadjiev
  */

#include <iostream>
#include <unordered_map>
#include <chrono>

#include "zmq_asio_socket.hpp"

#include <czmq.h>
#include <boost/bind.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/mutex.hpp>
#include <boost/fiber/asio/loop.hpp>
#include <boost/fiber/asio/spawn.hpp>
#include <boost/fiber/asio/yield.hpp>



const int num_fibers = 1000;
//const int max_running = 300;
//const int min_running = 100;
//const int threshold = 100;
//bool blocked = false;
//boost::fibers::promise<bool> main_wait;
int ready = 0;
//int launched = 0;
std::unordered_map<unsigned int, boost::fibers::promise<std::string>> promises;
bool stop = false;

//boost::fibers::condition_variable cond;
//boost::fibers::mutex mtx;
//bool data_ready = true;

void process_query(std::string& id, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
//	std::cout << id << " starts\n";
//	boost::system::error_code ec;
//	if((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
//		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
//	}
//	std::cout << "Sending SMT query to sock\n";
	zstr_sendm(service, id.c_str());
	int rc = zstr_send(service, query.c_str());
	assert(rc == 0);
//	std::cout << id << " Sent query\n";

	boost::fibers::promise<std::string> p;
	boost::fibers::future<std::string> f = p.get_future();
	promises.insert(std::make_pair(std::stoi(id), std::move(p)));

//	boost::fibers::future_status status = f.wait_for(std::chrono::seconds(15));
//
//	if(status == boost::fibers::future_status::ready){
		std::string ans = f.get();

		//	std::cout << id << " Receiving result\n";
		if (ans.empty()){
			std::cerr << "Received \"null\" response from server\n";
		}
//		} else {
//			if(id.compare(response_id) != 0){
//				std::cout << "Mismatch: " << id << " != " << response << "\n";
//			} else {
//				std::cout << "Match: " << id << " == " << response << "\n";
//			}
//			std::cout << "Result number " << response_id << ":\n" << result << "\n";
//		}
//	} else {
//		std::cout << id << " timed out\n";
//	}


	promises.erase(std::stoi(id));
//	std::cout << id << "'s response is:\n" << ans << "\n";
	++ready;
	if(ready%5000 == 0){
		std::cout << ready << " ready\n";
	}
//	if((!data_ready) && ((launched - ready) <= min_running)){
//		std::cout << id << " Notify reader\n";
//		data_ready = true;
//		cond.notify_all();
//	}
	if(ready == num_fibers){
		stop = true;
		asio_sock.get_io_service().stop();
		std::cout << "Stopped IO Service, unfulfilled promises left: " << promises.size() << "\n";
	}
//	std::cout << id << " exits (" << launched << " " << ready << ")\n";
//	if(blocked && ((launched - ready) < (max_running - threshold))){
//		main_wait.set_value(true);
//		blocked = false;
//	}
//	std::cout << "Fiber " << query << " exits (ready = " << ready << ")\n";
}

void reader(zmq_asio_socket& asio_sock, zsock_t* service, boost::fibers::asio::yield_context yield){
//	std::cout << "Reader starts\n";
	boost::system::error_code ec;
//	int read = 0;
	while(!stop){
//		if ((ready + read) >= num_fibers){
//			break;
//		}
//		read = 0;
		if(!(zsock_events(service) & ZMQ_POLLIN)){
			std::cout << "async read\n";
			asio_sock.async_read_some(boost::asio::null_buffers(), yield[ec]);
		} else {
			do{
	//			std::cout << "Reader reading\n";
				zmsg_t* msg = zmsg_recv(service);
				char* response_id = zmsg_popstr(msg);
				char* response = zmsg_popstr(msg);
				zmsg_destroy(&msg);

				auto elem = promises.find(std::stoi(response_id));

				if ( elem == promises.end() ){
					std::cout << "Promise not found\n";
				}
				else {
					elem->second.set_value(std::string(response));
				}

				zstr_free(&response_id);
				zstr_free(&response);
//				++read;
//				if(read % 500 == 0){
					boost::this_fiber::yield();
//				}
	//			std::cout << "Reader finished\n";
			}while(zsock_events(service) & ZMQ_POLLIN);
		}
		boost::this_fiber::yield();
	}
//	std::cout << "Reader exits\n";
}

void main_fiber(zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
//	std::cout << "Main fiber starts\n";
//	boost::this_fiber::yield();
	for(int i = 1; i<num_fibers; ++i){
		boost::fibers::fiber(boost::bind(process_query, std::to_string(i), std::ref(asio_sock), service, std::ref(query))).detach();
//		launched = i;
		boost::this_fiber::yield();
//		if(i%500 == 0 && i != 0){
			if(i%5000 == 0){
				std::cout << "Sent " << i << "\n";
			}
//		}
//		while((i - ready) > max_running){
//			data_ready = false;
//			mtx.lock();
//			while (!data_ready) {
//				std::cout << "Main fiber blocks\n";
//				cond.wait(mtx);
//			}
//			mtx.unlock();
//			std::cout << "Main fiber unblocks\n";
//			boost::this_fiber::yield();
//			std::cout << launched << " " << ready << "\n";
//			blocked = true;
//			boost::fibers::promise<bool> tmp;
//			std::swap(main_wait, tmp);
//			boost::fibers::future<bool> f = main_wait.get_future();
//			f.get();
//			blocked = false;
//		}
	}
	boost::fibers::fiber last_one(boost::bind(process_query, std::to_string(num_fibers), std::ref(asio_sock), service, std::ref(query)));
	last_one.join();
//	std::cout << "Main fiber" << boost::this_fiber::get_id() << " exits\n";
	std::cout << "Main fiber exits\n";
}

 int main(int argc, char* argv[]){
 	std::string smt_query(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());

	zsock_t* service = zsock_new(ZMQ_DEALER);
	assert(service);

	zsock_t* discovery = zsock_new_req("tcp://10.232.107.213:6789");
	assert(discovery);

 	std::cout << "client1 starting...\n";

 	int rc = zstr_send(discovery, "DISC");
 	assert(rc == 0);
 	std::cout << "Sent discovery request\n";
	zframe_t* rep = zframe_recv(discovery);
	zhashx_t* solvers = zhashx_unpack(rep);
	zframe_destroy(&rep);
	std::cout << "Service endpoint(s) discovered: {\n";
	for(void* solver = zhashx_first(solvers); solver != NULL; solver = zhashx_next(solvers)){
		std::string solver_addr = (const char*)zhashx_cursor(solvers);
		zsock_connect(service, "%s%s", "tcp://", solver_addr.c_str());
		std::cout << '\t' << solver_addr << "\n";
	}
	std::cout << "}\n";
	zsock_destroy(&discovery);

    //---------------------------------//

//	int zfd = zsock_fd(service);

	boost::asio::io_service ios;

	zmq_asio_socket asio_sock(service, ios);
	asio_sock.non_blocking(true);

	boost::fibers::asio::spawn(asio_sock.get_io_service(), boost::bind(reader, std::ref(asio_sock), service, _1));
	boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, std::ref(ios)));

    boost::fibers::fiber(boost::bind(main_fiber, std::ref(asio_sock), service, std::ref(smt_query))).detach();

//    boost::fibers::asio::spawn(ios, boost::bind(reader, std::ref(ios), std::ref(asio_sock), service, _1));
//    for(int i = 0; i<num_fibers; ++i){
//    	boost::fibers::fiber(boost::bind(process_query, std::to_string(i), std::ref(asio_sock), service, std::ref(smt_query))).detach();
//    }

	f.join();

	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
	asio_sock.close();
 	zsock_destroy(&service);
 	std::cout << "client1 exiting...\n";
 	return 0;
 }
