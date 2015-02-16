/*
 * client.cpp
 *
 *  Created on: Jan 22, 2015
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

const int num_fibers = 100;
int ready = 0;
char* id = NULL;
char* response = NULL;

void process_query(zmq_asio_socket& asio_sock, zsock_t* service, std::string& query, boost::fibers::asio::yield_context yield){
//	std::cout << "Sending query number: " << id << "\n";
	boost::system::error_code ec;
	if((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
	}
//	std::cout << "Sending SMT query to sock\n";
	zstr_sendm(service, query.c_str());
	int rc = zstr_send(service, query.c_str());
	assert(rc == 0);

//	std::cout << "Sent query " << query << "\n";
	while((id == NULL) || (query.compare(id) != 0)){
		boost::this_fiber::yield();
	}
//	std::cout << "Receiving result number: " << id << "\n";
	if (response == NULL){
		std::cerr << "Received \"null\" response from server\n";
	} else {
		if(query.compare(response) != 0){
			std::cout << "Mismatch: " << query << " != " << response << "\n";
		} else {
			std::cout << "Match: " << query << " == " << response << "\n";
		}
//		std::cout << "Result number " << id << ":\n" << result << "\n";
	}
	zstr_free(&id);
	zstr_free(&response);
	id = NULL;
	response = NULL;
	++ready;
//	std::cout << "Fiber " << query << " exits (ready = " << ready << ")\n";
}

void reader(boost::asio::io_service& ios, zmq_asio_socket& asio_sock, zsock_t* service, boost::fibers::asio::yield_context yield){
	boost::this_fiber::yield();
	boost::system::error_code ec;
	while(true){
		if (ready >= num_fibers){
			ios.stop();
			break;
		}
		while(id != NULL){
			boost::this_fiber::yield();
		}
		if((zsock_events(service) & ZMQ_POLLIN) != ZMQ_POLLIN){
			boost::asio::async_read(asio_sock, boost::asio::null_buffers(), boost::fibers::asio::yield[ec]);
		}
		zmsg_t* msg = zmsg_recv(service);
		id = zmsg_popstr(msg);
		response = zmsg_popstr(msg);
		zmsg_destroy(&msg);
//		std::cout << "Received result: " << id << "\n";
		boost::this_fiber::yield();
	}
}

void main_fiber(boost::asio::io_service& ios, zmq_asio_socket& asio_sock, zsock_t* service, boost::fibers::asio::yield_context yield){
	boost::fibers::asio::spawn(ios, boost::bind(reader, boost::ref(ios), boost::ref(asio_sock), service, _1));
	for(int i = 0; i<num_fibers; ++i){
		boost::fibers::asio::spawn(ios, boost::bind(process_query, boost::ref(asio_sock), service, boost::to_string(i), _1));
		if(i%500 == 0 && i != 0){
			boost::this_fiber::yield();
		}
	}
//	std::cout << "Main fiber" << boost::this_fiber::get_id() << " exits\n";
}

int main(int argc, char* argv[]){
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

    boost::fibers::asio::spawn(ios, boost::bind(main_fiber, boost::ref(ios), boost::ref(asio_sock), service, _1));

	boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, boost::ref(ios)));
	f.join();

	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
	asio_sock.close();
	zsock_destroy(&service);
	std::cout << "client1 exiting...\n";
	return 0;
}
