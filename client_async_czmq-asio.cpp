/*
 * client_async.cpp
 *
 *  Created on: Feb 10, 2015
 *      Author: rakadjiev
 */


#include <czmq.h>
//#include <azmq/socket.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <fstream>

class non_closing_service : public boost::asio::posix::stream_descriptor_service{
public:
	explicit non_closing_service(boost::asio::io_service& io_service) : boost::asio::posix::stream_descriptor_service(io_service){}
	void destroy(typename boost::asio::posix::stream_descriptor_service::implementation_type& impl){}
	boost::system::error_code close(boost::asio::detail::reactive_descriptor_service::implementation_type& impl, boost::system::error_code& ec){
		return ec;
	}
};

typedef boost::asio::posix::basic_stream_descriptor<non_closing_service> zmq_asio_socket;

void get_reply(zmq_asio_socket& asio_sock, zsock_t* service){
	if((zsock_events(service) & ZMQ_POLLIN) != ZMQ_POLLIN){
		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), boost::bind(get_reply, boost::ref(asio_sock), boost::ref(service)));
	} else {
		char* ans = zstr_recv(service);
		if (ans == NULL){
			std::cerr << "Received \"null\" response from server\n";
		} else {
			std::string result = ans;
			zstr_free(&ans);
		}
	}
}

void send_query(boost::asio::io_service& ios, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
	if((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
		boost::asio::async_read(asio_sock, boost::asio::null_buffers(), boost::bind(send_query, boost::ref(ios), boost::ref(asio_sock), boost::ref(service), boost::ref(query)));
	} else {
		int rc = zstr_send(service, query.c_str());
		assert(rc == 0);
		ios.post(boost::bind(get_reply, boost::ref(asio_sock), boost::ref(service)));
	}
}

void some_work(boost::asio::io_service& ios, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
	for(int i = 0; i<10000; ++i){
		ios.post(boost::bind(send_query, boost::ref(ios), boost::ref(asio_sock), boost::ref(service), boost::ref(query)));
	}
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

	ios.post(boost::bind(some_work, boost::ref(ios), boost::ref(asio_sock), boost::ref(service), boost::ref(smt_query)));

    ios.run();

    zhashx_destroy(&solvers);
    zsock_set_linger(service, 0);
    asio_sock.close();
    zsock_destroy(&service);
    std::cout << "client1 exiting...\n";
    return 0;
}

