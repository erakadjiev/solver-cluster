/*
 * client_async_fibers.cpp
 *
 *  Created on: Feb 15, 2015
 *      Author: rakadjiev
 */

#include <czmq.h>
//#include <azmq/socket.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/fiber/all.hpp>
#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/loop.hpp>
#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/spawn.hpp>
#include </usr/local/boost_1_57_0/libs/fiber/examples/asio/yield.hpp>
#include <iostream>

class non_closing_service : public boost::asio::posix::stream_descriptor_service{
public:
	explicit non_closing_service(boost::asio::io_service& io_service) : boost::asio::posix::stream_descriptor_service(io_service){}
	void destroy(typename boost::asio::posix::stream_descriptor_service::implementation_type& impl){}
	boost::system::error_code close(boost::asio::detail::reactive_descriptor_service::implementation_type& impl, boost::system::error_code& ec){
		return ec;
	}
};

typedef boost::asio::posix::basic_stream_descriptor<non_closing_service> zmq_asio_socket;

void process_query(int& ready, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query, boost::fibers::asio::yield_context yield){
//	std::cout << "Sending query number: " << id << "\n";
	boost::system::error_code ec;
	while((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
	}
//	std::cout << "Sending SMT query to sock\n";
	int rc = zstr_send(service, query.c_str());
	assert(rc == 0);

	while((zsock_events(service) & ZMQ_POLLIN) != ZMQ_POLLIN){
		boost::asio::async_read(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
	}
//	std::cout << "Receiving result number: " << id << "\n";
	char* ans = zstr_recv(service);
	if (ans == NULL){
		std::cerr << "Received \"null\" response from server\n";
	} else {
		std::string result = ans;
		zstr_free(&ans);
//		std::cout << "Result number " << id << ":\n" << result << "\n";
	}
	++ready;
}

void main_fiber(boost::asio::io_service& ios, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query, boost::fibers::asio::yield_context yield){
	int ready = 0;
	for(int i = 0; i<10000; ++i){
		boost::fibers::asio::spawn(ios, boost::bind(process_query, boost::ref(ready), boost::ref(asio_sock), boost::ref(service), boost::ref(query), _1));
		if(i%500 == 0 && i != 0){
			boost::this_fiber::yield();
		}
	}
	while(ready < 10000){
		boost::this_fiber::yield();
	}
	ios.stop();
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

    boost::fibers::asio::spawn(ios, boost::bind(main_fiber, boost::ref(ios), boost::ref(asio_sock), boost::ref(service), boost::ref(smt_query), _1));

	boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, boost::ref(ios)));
	f.join();

	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
	asio_sock.close();
	zsock_destroy(&service);
	std::cout << "client1 exiting...\n";
	return 0;
}
