#include <btcnew/secure/common.hpp>
#include <btcnew/secure/epoch.hpp>

#include <gtest/gtest.h>

TEST (epochs, is_epoch_link)
{
	btcnew::epochs epochs;
	// Test epoch 1
	btcnew::keypair key1;
	auto link1 = 42;
	auto link2 = 43;
	ASSERT_FALSE (epochs.is_epoch_link (link1));
	ASSERT_FALSE (epochs.is_epoch_link (link2));
	epochs.add (btcnew::epoch::epoch_1, key1.pub, link1);
	ASSERT_TRUE (epochs.is_epoch_link (link1));
	ASSERT_FALSE (epochs.is_epoch_link (link2));
	ASSERT_EQ (key1.pub, epochs.signer (btcnew::epoch::epoch_1));
	ASSERT_EQ (epochs.epoch (link1), btcnew::epoch::epoch_1);

	// Test epoch 2
	btcnew::keypair key2;
	epochs.add (btcnew::epoch::epoch_2, key2.pub, link2);
	ASSERT_TRUE (epochs.is_epoch_link (link2));
	ASSERT_EQ (key2.pub, epochs.signer (btcnew::epoch::epoch_2));
	ASSERT_EQ (btcnew::uint256_union (link1), epochs.link (btcnew::epoch::epoch_1));
	ASSERT_EQ (btcnew::uint256_union (link2), epochs.link (btcnew::epoch::epoch_2));
	ASSERT_EQ (epochs.epoch (link2), btcnew::epoch::epoch_2);
}

TEST (epochs, is_sequential)
{
	ASSERT_TRUE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_0, btcnew::epoch::epoch_1));
	ASSERT_TRUE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_1, btcnew::epoch::epoch_2));

	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_0, btcnew::epoch::epoch_2));
	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_0, btcnew::epoch::invalid));
	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::unspecified, btcnew::epoch::epoch_1));
	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_1, btcnew::epoch::epoch_0));
	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_2, btcnew::epoch::epoch_0));
	ASSERT_FALSE (btcnew::epochs::is_sequential (btcnew::epoch::epoch_2, btcnew::epoch::epoch_2));
}
