#include <btcnew/lib/memory.hpp>
#include <btcnew/secure/common.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace
{
/** This allocator records the size of all allocations that happen */
template <class T>
class record_allocations_new_delete_allocator
{
public:
	using value_type = T;

	explicit record_allocations_new_delete_allocator (std::vector<size_t> * allocated) :
	allocated (allocated)
	{
	}

	template <typename U>
	record_allocations_new_delete_allocator (const record_allocations_new_delete_allocator<U> & a)
	{
		allocated = a.allocated;
	}

	template <typename U>
	record_allocations_new_delete_allocator & operator= (const record_allocations_new_delete_allocator<U> &) = delete;

	T * allocate (size_t num_to_allocate)
	{
		auto size_allocated = (sizeof (T) * num_to_allocate);
		allocated->push_back (size_allocated);
		return static_cast<T *> (::operator new (size_allocated));
	}

	void deallocate (T * p, size_t num_to_deallocate)
	{
		::operator delete (p);
	}

	std::vector<size_t> * allocated;
};

template <typename T>
size_t get_allocated_size ()
{
	std::vector<size_t> allocated;
	record_allocations_new_delete_allocator<T> alloc (&allocated);
	std::allocate_shared<T, record_allocations_new_delete_allocator<T>> (alloc);
	assert (allocated.size () == 1);
	return allocated.front ();
}
}

TEST (memory_pool, validate_cleanup)
{
	// This might be turned off, e.g on Mac for instance, so don't do this test
	if (!btcnew::get_use_memory_pools ())
	{
		return;
	}

	btcnew::make_shared<btcnew::open_block> ();
	btcnew::make_shared<btcnew::receive_block> ();
	btcnew::make_shared<btcnew::send_block> ();
	btcnew::make_shared<btcnew::change_block> ();
	btcnew::make_shared<btcnew::state_block> ();
	btcnew::make_shared<btcnew::vote> ();

	ASSERT_TRUE (btcnew::purge_singleton_pool_memory<btcnew::open_block> ());
	ASSERT_TRUE (btcnew::purge_singleton_pool_memory<btcnew::receive_block> ());
	ASSERT_TRUE (btcnew::purge_singleton_pool_memory<btcnew::send_block> ());
	ASSERT_TRUE (btcnew::purge_singleton_pool_memory<btcnew::state_block> ());
	ASSERT_TRUE (btcnew::purge_singleton_pool_memory<btcnew::vote> ());

	// Change blocks have the same size as open_block so won't deallocate any memory
	ASSERT_FALSE (btcnew::purge_singleton_pool_memory<btcnew::change_block> ());

	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::open_block> (), get_allocated_size<btcnew::open_block> () - sizeof (size_t));
	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::receive_block> (), get_allocated_size<btcnew::receive_block> () - sizeof (size_t));
	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::send_block> (), get_allocated_size<btcnew::send_block> () - sizeof (size_t));
	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::change_block> (), get_allocated_size<btcnew::change_block> () - sizeof (size_t));
	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::state_block> (), get_allocated_size<btcnew::state_block> () - sizeof (size_t));
	ASSERT_EQ (btcnew::determine_shared_ptr_pool_size<btcnew::vote> (), get_allocated_size<btcnew::vote> () - sizeof (size_t));
}
