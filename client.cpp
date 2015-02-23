 /*
  * client.cpp
  *
  *  Created on: Jan 22, 2015
  *      Author: rakadjiev
  */

#include <iostream>
#include <unordered_map>

#include "zmq_asio_socket.hpp"

#include <czmq.h>
#include <boost/bind.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/asio/loop.hpp>
#include <boost/fiber/asio/spawn.hpp>
#include <boost/fiber/asio/yield.hpp>



const int num_fibers = 100000;
int ready = 0;
std::unordered_map<unsigned int, boost::fibers::promise<std::string>> promises;

void process_query(std::string& id, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
//	std::cout << id << " Sending query \n";
	boost::system::error_code ec;
//	if((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
//		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
//	}
//	std::cout << "Sending SMT query to sock\n";
	zstr_sendm(service, id.c_str());
	int rc = zstr_send(service, query.c_str());
	assert(rc == 0);
//	std::cout << "Sent query " << query << "\n";

	boost::fibers::promise<std::string> p;
	boost::fibers::future<std::string> f = p.get_future();
	promises.insert(std::make_pair(std::stoi(id), std::move(p)));
	std::string ans = f.get();

	promises.erase(std::stoi(id));

//	std::cout << id << " Receiving result\n";
	if (ans.empty()){
		std::cerr << "Received \"null\" response from server\n";
	}
//	} else {
//		if(id.compare(response_id) != 0){
//			std::cout << "Mismatch: " << id << " != " << response << "\n";
//		} else {
//			std::cout << "Match: " << id << " == " << response << "\n";
//		}
//		std::cout << "Result number " << response_id << ":\n" << result << "\n";
//	}
//	std::cout << id << "'s response is:\n" << ans << "\n";
	++ready;
//	std::cout << "Fiber " << query << " exits (ready = " << ready << ")\n";
}

void reader(zmq_asio_socket& asio_sock, zsock_t* service, boost::fibers::asio::yield_context yield){
	boost::system::error_code ec;
	while(true){
		if (ready >= num_fibers){
			asio_sock.get_io_service().stop();
			std::cout << "Stopped IO Service, unfulfilled promises left: " << promises.size() << "\n";
			break;
		}

		int read = 0;
		boost::asio::async_read(asio_sock, boost::asio::null_buffers(), yield[ec]);
		while((zsock_events(service) & ZMQ_POLLIN) == ZMQ_POLLIN){
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
			++read;
			if(read % 500 == 0){
				boost::this_fiber::yield();
			}
		}
	}
}

void main_fiber(zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
	boost::fibers::asio::spawn(asio_sock.get_io_service(), boost::bind(reader, std::ref(asio_sock), service, _1));
	for(int i = 0; i<num_fibers; ++i){
		boost::fibers::fiber(boost::bind(process_query, std::to_string(i), std::ref(asio_sock), service, std::ref(query))).detach();
		if(i%500 == 0 && i != 0){
			if(i%5000 == 0 && i != 0){
				std::cout << "Sent " << i << "\n";
			}
			boost::this_fiber::yield();
		}
	}
//	std::cout << "Main fiber" << boost::this_fiber::get_id() << " exits\n";
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

	int zfd = zsock_fd(service);

	boost::asio::io_service ios;

	zmq_asio_socket asio_sock(ios, zfd);
    asio_sock.non_blocking(true);

    boost::fibers::fiber(boost::bind(main_fiber, std::ref(asio_sock), service, std::ref(smt_query))).detach();

//    boost::fibers::asio::spawn(ios, boost::bind(reader, std::ref(ios), std::ref(asio_sock), service, _1));
//    for(int i = 0; i<num_fibers; ++i){
//    	boost::fibers::fiber(boost::bind(process_query, std::to_string(i), std::ref(asio_sock), service, std::ref(smt_query))).detach();
//    }

    boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, std::ref(ios)));
	f.join();

	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
	asio_sock.close();
 	zsock_destroy(&service);
 	std::cout << "client1 exiting...\n";
 	return 0;
 }
