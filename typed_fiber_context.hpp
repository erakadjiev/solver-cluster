/*
 * typed_fiber_context.hpp
 *
 *  Created on: Feb 23, 2015
 *      Author: rakadjiev
 */

#ifndef TYPED_FIBER_CONTEXT_HPP_
#define TYPED_FIBER_CONTEXT_HPP_

#include <boost/fiber/fiber_context.hpp>

class typed_fiber_context : public boost::fibers::fiber_context{
public:
	enum class fiber_type{
		NONE,
		MAIN,
		READER,
		SENDER,
		RECEIVER,
		WORKER
	};

	void print_type(){
		std::cout << "Fiber context type: ";
		switch(type){
		case fiber_type::MAIN:
			std::cout << "main\n";
			break;
		case fiber_type::READER:
			std::cout << "reader\n";
			break;
		case fiber_type::SENDER:
			std::cout << "sender\n";
			break;
		case fiber_type::RECEIVER:
			std::cout << "receiver\n";
			break;
		case fiber_type::WORKER:
			std::cout << "worker\n";
			break;
		}

	}

	fiber_type get_type() const noexcept{
		return type;
	}

	void set_type(fiber_type _type) noexcept{
		type = _type;
	}

	template< typename StackAlloc, typename Fn, typename ... Args >
	    explicit typed_fiber_context(fiber_type type, boost::context::preallocated palloc, StackAlloc salloc,
	    		Fn && fn, Args && ... args) : fiber_context(palloc, salloc, std::forward<Fn>(fn), std::forward<Args>(args) ...), type(type){}
private:
	fiber_type type;
};

#endif /* TYPED_FIBER_CONTEXT_HPP_ */
