/*
 * client.cpp
 *
 *  Created on: Jan 22, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>
#include <azmq/socket.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/fiber/all.hpp>
//#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/loop.hpp>
#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/spawn.hpp>
#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/yield.hpp>
#include <iostream>


void process_query(azmq::dealer_socket& service, int id, std::string& query, boost::fibers::asio::yield_context yield){
	std::cout << "Sending query number: " << id << "\n";
	boost::system::error_code ec;
	service.async_send(boost::asio::buffer(query), boost::fibers::asio::yield[ec]);

	std::array<char, 10240> data;
	boost::asio::mutable_buffers_1 b(data.data(), data.size());
	std::cout << "Receiving result number: " << id << "\n";
	service.async_receive(b, boost::fibers::asio::yield[ec]);

	if(ec){
		std::cout << ec.message() << "\n";
	} else {
		std::cout << "Result number " << id << ":\n" << boost::asio::buffer_cast<char*>(b) << "\n";
	}
}

void run_service(boost::asio::io_service& ios) {
	ios.run();
}

int main(int argc, char* argv[]){
	std::string smt_query(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());

	boost::asio::io_service ios;

	azmq::dealer_socket service(ios, true);

	azmq::req_socket discovery(ios, true);
	discovery.connect("tcp://10.232.107.213:6789");

	std::cout << "client1 starting...\n";

	discovery.send(boost::asio::buffer("DISC"));

	std::cout << "Sent discovery request\n";
	azmq::message m;
	discovery.receive(m);
	boost::asio::const_buffer b =  m.buffer();
	zframe_t* t = zframe_new(boost::asio::buffer_cast<const zmq_msg_t*>(b), boost::asio::buffer_size(b));
	zhashx_t* solvers = zhashx_unpack(t);
    zframe_destroy(&t);
    std::cout << "Service endpoint(s) discovered: {\n";
    for(void* solver = zhashx_first(solvers); solver != NULL; solver = zhashx_next(solvers)){
    	std::string solver_addr = (const char*)zhashx_cursor(solvers);
    	service.connect("tcp://" + solver_addr);
    	std::cout << '\t' << solver_addr << "\n";
    }
    std::cout << "}\n";

    //---------------------------------//


    for(int i = 0; i<5; ++i){
		boost::fibers::asio::spawn(ios, boost::bind(process_query, i, boost::ref(service), boost::ref(smt_query), _1));
    }

	boost::fibers::fiber f(boost::bind(run_service, boost::ref(ios)));
	f.join();

	zhashx_destroy(&solvers);
	std::cout << "client1 exiting...\n";
	return 0;
}
