/*
 * typed_fiber.hpp
 *
 *  Created on: Feb 23, 2015
 *      Author: rakadjiev
 */

#ifndef TYPED_FIBER_HPP_
#define TYPED_FIBER_HPP_

#include <boost/fiber/fiber.hpp>
#include "typed_fiber_context.hpp"

class typed_fiber : public boost::fibers::fiber{
protected:
	template< typename StackAlloc, typename Fn, typename ... Args >
	static ptr_t create(typed_fiber_context::fiber_type fiber_type, StackAlloc salloc, Fn && fn, Args && ... args) {
		boost::context::stack_context sctx( salloc.allocate() );
		// reserve space for control structure
		std::size_t size = sctx.size - sizeof(typed_fiber_context);
		void * sp = static_cast< char * >( sctx.sp) - sizeof(typed_fiber_context);
		// placement new of worker_fiber on top of fiber's stack
		return ptr_t(
				new ( sp) typed_fiber_context(fiber_type, boost::context::preallocated( sp, size, sctx), salloc,
						std::forward< Fn >( fn), std::forward< Args >( args) ... ) );
	}
public:
	template< typename Fn, typename ... Args >
	explicit typed_fiber(typed_fiber_context::fiber_type fiber_type, Fn && fn, Args && ... args) :
	typed_fiber(fiber_type, std::allocator_arg, boost::context::fixedsize_stack(),
			std::forward< Fn >( fn), std::forward< Args >( args) ... ) {
	}

	template< typename StackAllocator, typename Fn, typename ... Args >
	explicit typed_fiber(typed_fiber_context::fiber_type fiber_type, std::allocator_arg_t, StackAllocator salloc, Fn && fn, Args && ... args){
		impl_ = create(fiber_type, salloc, std::forward< Fn >( fn), std::forward< Args >( args) ... );
		start_();
	}

	template< typename Fn, typename ... Args >
	explicit typed_fiber(Fn && fn, Args && ... args) :
	typed_fiber(typed_fiber_context::fiber_type::WORKER, std::allocator_arg, boost::context::fixedsize_stack(),
			std::forward< Fn >( fn), std::forward< Args >( args) ... ) {
	}

	template< typename StackAllocator, typename Fn, typename ... Args >
	explicit typed_fiber(std::allocator_arg_t, StackAllocator salloc, Fn && fn, Args && ... args){
		impl_ = create(typed_fiber_context::fiber_type::WORKER, salloc, std::forward< Fn >( fn), std::forward< Args >( args) ... );
		start_();
	}
};

struct typed_fiber_creator{

	typed_fiber_creator(typed_fiber_context::fiber_type fiber_type) : fiber_type(fiber_type){}

	template<typename Function>
	boost::fibers::fiber operator()(Function&& func){
		return typed_fiber(fiber_type, std::forward<Function>(func));
	}

	typed_fiber_context::fiber_type fiber_type;

};

#endif /* TYPED_FIBER_HPP_ */
