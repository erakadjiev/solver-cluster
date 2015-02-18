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
#include <boost/fiber/all.hpp>
//#include </usr/local/boost_1_57_0_fiber/libs/fiber/examples/asio/loop.hpp>
//#include </usr/local/boost_1_57_0_fiber/libs/fiber/examples/asio/spawn.hpp>
//#include </usr/local/boost_1_57_0_fiber/libs/fiber/examples/asio/yield.hpp>
#include </home/rakadjiev/workspace/modular-boost/libs/fiber/examples/asio/loop.hpp>
#include </home/rakadjiev/workspace/modular-boost/libs/fiber/examples/asio/spawn.hpp>
#include </home/rakadjiev/workspace/modular-boost/libs/fiber/examples/asio/yield.hpp>
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

const int num_fibers = 2;
int ready = 0;
char* response_id = NULL;
char* response = NULL;

volatile int test = 0;

//void process_query(std::string& id, zmq_asio_socket& asio_sock, zsock_t* service, std::string& query, boost::fibers::asio::yield_context yield){
void process_query(std::string id, zsock_t* service, std::string& query){
//	std::cout << "Worker " << id << " is :" << boost::this_fiber::get_id() << "\n";
//	std::cout << "Sending query number: " << response_id << "\n";
	boost::system::error_code ec;
//	if((zsock_events(service) & ZMQ_POLLOUT) != ZMQ_POLLOUT){
//		boost::asio::async_write(asio_sock, boost::asio::null_buffers(), yield[ec]);
//	}
//	std::cout << "Sending SMT query to sock\n";
	zstr_sendm(service, id.c_str());
	int rc = zstr_send(service, query.c_str());
	assert(rc == 0);

//	std::cout << "Sent query " << query << "\n";
	while((response_id == NULL) || (id.compare(response_id) != 0)){
		boost::this_fiber::yield();
	}
//	std::cout << "Receiving result number: " << response_id << "\n";
	if (response == NULL){
		std::cerr << "Received \"null\" response from server\n";
	} /*else {
		if(query.compare(response) != 0){
			std::cout << "Mismatch: " << query << " != " << response << "\n";
		} else {
			std::cout << "Match: " << query << " == " << response << "\n";
		}
//		std::cout << "Result number " << response_id << ":\n" << result << "\n";
	}*/
	zstr_free(&response_id);
	zstr_free(&response);
	response_id = NULL;
	response = NULL;
	++ready;
	std::cout << "Fiber " << id << " exits (ready = " << ready << ")\n";
}

void reader(zmq_asio_socket& asio_sock, zsock_t* service, boost::fibers::asio::yield_context yield){
//	std::cout << "Reader is :" << boost::this_fiber::get_id() << "\n";
//	boost::this_fiber::yield();
	boost::system::error_code ec;
//	boost::asio::io_service& ios = asio_sock.get_io_service();
	while(ready < num_fibers){
		++test;
		std::cout << "Reader incr. test\n";
		if((zsock_events(service) & ZMQ_POLLIN) != ZMQ_POLLIN){
			std::cout << "Reader no pollin\n";
			boost::asio::async_read(asio_sock, boost::asio::null_buffers(), yield[ec]);
		}
		std::cout << "Reader readin\n";
		zmsg_t* msg = zmsg_recv(service);
		response_id = zmsg_popstr(msg);
		response = zmsg_popstr(msg);
		zmsg_destroy(&msg);
//		std::cout << "Received result: " << response_id << "\n";
		while(response_id != NULL){
			boost::this_fiber::yield();
		}
	}
	boost::asio::io_service& ios = asio_sock.get_io_service();
//	while(!ios.stopped()){
//		asio_sock.close();
		ios.stop();
//	}
	std::cout << "Reader exits\n";
}

void main_fiber(zmq_asio_socket& asio_sock, zsock_t* service, std::string& query){
//	std::cout << "Main is :" << boost::this_fiber::get_id() << "\n";
//	std::cout << "Main 2\n";
//	std::cout << "Main 3\n";
	for(int i = 0; i<num_fibers; ++i){
		boost::fibers::fiber(boost::bind(process_query, std::to_string(i), service, std::ref(query))).detach();
//		std::cout << "Main 4\n";
//		boost::fibers::fiber(boost::bind(process_query, std::to_string(i), service, std::ref(query))).detach();
//		boost::fibers::asio::spawn(ios, boost::bind(process_query, std::to_string(i), std::ref(asio_sock), service, std::ref(query), _1));
//		std::cout << "Main 5\n";
//		if(i%500 == 0 && i != 0){
//			boost::this_fiber::yield();
//		}
	}
	++test;
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
	try	{
	int zfd = zsock_fd(service);

	boost::asio::io_service ios;
	boost::asio::io_service::work work(ios);

	zmq_asio_socket asio_sock(ios, zfd);
    asio_sock.non_blocking(true);

    boost::fibers::fiber(boost::bind(main_fiber, std::ref(asio_sock), service, std::ref(smt_query))).detach();
	boost::fibers::asio::spawn(asio_sock.get_io_service(), boost::bind(reader, std::ref(asio_sock), service, _1));

    boost::fibers::fiber ios_runner(boost::bind(boost::fibers::asio::run_service, std::ref(ios)));
    std::cout << "Join IO Runner\n";
    ios_runner.join();
    std::cout << "IO Runner finished\n";

	zhashx_destroy(&solvers);
	zsock_set_linger(service, 0);
	asio_sock.close();
	zsock_destroy(&service);
	std::cout << "client1 exiting...\n";
	} catch ( std::exception const& e){
		std::cerr << "Exception: " << e.what() << "\n";
	}
	return 0;
}
