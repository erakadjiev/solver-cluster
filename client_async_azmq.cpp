/*
 * client_async.cpp
 *
 *  Created on: Feb 10, 2015
 *      Author: rakadjiev
 */


#include <czmq.h>
#include <azmq/socket.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <fstream>

boost::asio::io_service ios;
azmq::dealer_socket service(ios, true);

void print_reply(const boost::system::error_code& e, char* data){
	if(e){
		std::cout << e.message() << "\n";
	}
	delete[] data;
}

void get_reply(const boost::system::error_code& e){
	char* data = new char[2000]();
	boost::asio::mutable_buffers_1 b(data, 2000);
	service.async_receive(b, boost::bind(print_reply, boost::asio::placeholders::error, data));
}

int main(int argc, char* argv[]){
	std::string smt_query(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());


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

    for(int i = 0; i<10000; ++i){
    	service.async_send(boost::asio::buffer(smt_query), boost::bind(get_reply, boost::asio::placeholders::error));
    }

    ios.run();

	zhashx_destroy(&solvers);

	std::cout << "client1 exiting...\n";
	return 0;
}

