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
#include <vector>

boost::asio::io_service ios;
azmq::dealer_socket service(ios, true);

void print_reply(const boost::system::error_code& e, std::string que, char* data){
	if(e){
		std::cout << e.message() << "\n";
	}
	std::string rep = data;
	if(rep.compare(que) != 0){
		std::cout << "Mismatch: " << que << " != " << rep << "\n";
	}
	delete[] data;
}

//void print_reply1(const boost::system::error_code& e, char* data){
//	if(e){
//		std::cout << e.message() << "\n";
//	}
//	std::string rep = data;
//	if(rep.compare("0") != 0){
//		std::cout << "Mismatch: " << 0 << " != " << rep << "\n";
//	}
//	delete[] data;
//}
//
//void print_reply2(const boost::system::error_code& e, char* data){
//	if(e){
//		std::cout << e.message() << "\n";
//	}
//	std::string rep = data;
//	if(rep.compare("1") != 0){
//		std::cout << "Mismatch: " << 1 << " != " << rep << "\n";
//	}
//	delete[] data;
//}
//
//void print_reply3(const boost::system::error_code& e, char* data){
//	if(e){
//		std::cout << e.message() << "\n";
//	}
//	std::string rep = data;
//	if(rep.compare("2") != 0){
//		std::cout << "Mismatch: " << 2 << " != " << rep << "\n";
//	}
//	delete[] data;
//}
//
//void get_reply1(const boost::system::error_code& e){
//	char* data = new char[200]();
//	boost::asio::mutable_buffers_1 b(data, 200);
//	service.async_receive(b, boost::bind(print_reply1, boost::asio::placeholders::error, data));
//}
//
//void get_reply2(const boost::system::error_code& e){
//	char* data = new char[200]();
//	boost::asio::mutable_buffers_1 b(data, 200);
//	service.async_receive(b, boost::bind(print_reply2, boost::asio::placeholders::error, data));
//}
//
//void get_reply3(const boost::system::error_code& e){
//	char* data = new char[200]();
//	boost::asio::mutable_buffers_1 b(data, 200);
//	service.async_receive(b, boost::bind(print_reply3, boost::asio::placeholders::error, data));
//}

void get_reply(const boost::system::error_code& e, std::string que){
	char* data = new char[200]();
	boost::asio::mutable_buffers_1 b(data, 200);
	service.async_receive(b, boost::bind(print_reply, boost::asio::placeholders::error, que, data));
}

int main(int argc, char* argv[]){

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

    std::array<std::string, 100> data;
    std::vector<azmq::message> msg(100);
    msg.clear();

    for(int i = 0; i<100; ++i){
    	data[i] = std::to_string(i);
    	msg.emplace_back(data[i]);
    }

//    service.async_send(msg[0], boost::bind(get_reply1, boost::asio::placeholders::error));
//    service.async_send(msg[1], boost::bind(get_reply2, boost::asio::placeholders::error));
//    service.async_send(msg[2], boost::bind(get_reply3, boost::asio::placeholders::error));
    for(int i = 0; i<100; ++i){
    	service.async_send(msg[i], boost::bind(get_reply, boost::asio::placeholders::error, data[i]));
    }

//    for(int i = 0; i<3; ++i){
//    	std::cout << data[i] << " " << msg[i].string() << "\n";
//    }

    ios.run();

	zhashx_destroy(&solvers);

	std::cout << "client1 exiting...\n";
	return 0;
}

