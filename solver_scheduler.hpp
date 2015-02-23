/*
 * solver_scheduler.hpp
 *
 *  Created on: Feb 23, 2015
 *      Author: rakadjiev
 */

#ifndef SOLVER_SCHEDULER_HPP_
#define SOLVER_SCHEDULER_HPP_

#include <boost/fiber/fiber.hpp>
#include <boost/fiber/algorithm.hpp>

class solver_scheduler : public boost::fibers::sched_algorithm {
private:
	boost::fibers::fiber_context* main = nullptr;
	boost::fibers::fiber_context* reader = nullptr;
	boost::fibers::detail::fifo sender_queue;
	boost::fibers::detail::fifo receiver_queue;
	boost::fibers::detail::fifo worker_queue;

	typed_fiber_context::fiber_type previous_fiber = typed_fiber_context::fiber_type::NONE;

public:
	virtual void awakened(boost::fibers::fiber_context* f){
		BOOST_ASSERT( nullptr != f);
		if(typed_fiber_context* tf = dynamic_cast<typed_fiber_context*>(f)){
			switch(tf->get_type()){
			case typed_fiber_context::fiber_type::READER:
				reader = f;
//				std::cout << "SCHEDULER: reader awake \n";
				break;
			case typed_fiber_context::fiber_type::MAIN:
				main = f;
//				std::cout << "SCHEDULER: main awake \n";
				break;
			case typed_fiber_context::fiber_type::SENDER:
				sender_queue.push(f);
//				std::cout << "SCHEDULER: sender awake \n";
				break;
			case typed_fiber_context::fiber_type::RECEIVER:
//				std::cout << "SCHEDULER: receiver awake \n";
				receiver_queue.push(f);
				break;
			default:
				worker_queue.push(f);
//				std::cout << "SCHEDULER: worker awake \n";
				break;
			}
		} else {
//			std::cout << "SCHEDULER: non-typed worker awake \n";
			worker_queue.push(f);
		}
	}

	virtual boost::fibers::fiber_context* pick_next(){
		boost::fibers::fiber_context* victim(nullptr);
		if ((!receiver_queue.empty()) && (previous_fiber != typed_fiber_context::fiber_type::RECEIVER)){
			victim = receiver_queue.pop();
//			std::cout << "SCHEDULER: picked receiver \n";
			previous_fiber = typed_fiber_context::fiber_type::RECEIVER;
			BOOST_ASSERT( nullptr != victim);
		}
		else if ((reader != nullptr) && (previous_fiber != typed_fiber_context::fiber_type::READER)){
			victim = reader;
//			std::cout << "SCHEDULER: picked reader \n";
			previous_fiber = typed_fiber_context::fiber_type::READER;
			reader = nullptr;
		}
		else if ((!sender_queue.empty()) && (previous_fiber != typed_fiber_context::fiber_type::SENDER)){
			victim = sender_queue.pop();
//			std::cout << "SCHEDULER: picked sender \n";
			previous_fiber = typed_fiber_context::fiber_type::SENDER;
			BOOST_ASSERT( nullptr != victim);
		}
		else if ((main != nullptr) && (previous_fiber != typed_fiber_context::fiber_type::MAIN)) {
			victim = main;
//			std::cout << "SCHEDULER: picked main \n";
			previous_fiber = typed_fiber_context::fiber_type::MAIN;
			main = nullptr;
		}
		else if (!worker_queue.empty()) {
			victim = worker_queue.pop();
//			std::cout << "SCHEDULER: picked worker \n";
			previous_fiber = typed_fiber_context::fiber_type::WORKER;
			BOOST_ASSERT( nullptr != victim);
		}
		if((victim == nullptr) && (previous_fiber != typed_fiber_context::fiber_type::NONE)){
			previous_fiber = typed_fiber_context::fiber_type::NONE;
			return pick_next();
		}
		return victim;
	}
};

#endif /* SOLVER_SCHEDULER_HPP_ */
