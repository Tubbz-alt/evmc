// EVMC: Ethereum Client-VM Connector API.
// Copyright 2020 The EVMC Authors.
// Licensed under the Apache License, Version 2.0.

#include "../../examples/example_vm/example_vm.h"
#include <evmc/evmc.hpp>
#include <evmc/mocked_host.hpp>
#include <tools/utils/utils.hpp>
#include <gtest/gtest.h>
#include <cstring>

using namespace evmc::literals;

namespace
{
struct ExpectedOutput
{
    evmc::bytes bytes;

    explicit ExpectedOutput(const char* output_hex) noexcept : bytes{evmc::from_hex(output_hex)} {}

    friend bool operator==(const evmc::result& result, const ExpectedOutput& expected) noexcept
    {
        return expected.bytes.compare(0, evmc::bytes::npos, result.output_data,
                                      result.output_size) == 0;
    }
};

auto vm = evmc::VM{evmc_create_example_vm()};

class example_vm : public testing::Test
{
protected:
    evmc::MockedHost host;

    evmc_message msg{};

    example_vm() noexcept
    {
        msg.sender = 0x5000000000000000000000000000000000000005_address;
        msg.destination = 0xd00000000000000000000000000000000000000d_address;
    }

    evmc::result execute_in_example_vm(int64_t gas,
                                       const char* code_hex,
                                       const char* input_hex = "") noexcept
    {
        const auto code = evmc::from_hex(code_hex);
        const auto input = evmc::from_hex(input_hex);

        msg.gas = gas;
        msg.input_data = input.data();
        msg.input_size = input.size();

        return vm.execute(host, EVMC_FRONTIER, msg, code.data(), code.size());
    }
};


}  // namespace

TEST_F(example_vm, return_address)
{
    // Assembly: `{ mstore(0, address()) return(12, 20) }`.
    const auto r = execute_in_example_vm(6, "306000526014600cf3", "");
    EXPECT_EQ(r.status_code, EVMC_SUCCESS);
    EXPECT_EQ(r.gas_left, 0);
    EXPECT_EQ(r, ExpectedOutput("d00000000000000000000000000000000000000d"));
}

TEST_F(example_vm, counter_in_storage)
{
    // Assembly: `{ sstore(0, add(sload(0), 1)) }`

    auto& storage_value = host.accounts[msg.destination].storage[{}].value;
    storage_value = 0x00000000000000000000000000000000000000000000000000000000000000bb_bytes32;
    const auto r = execute_in_example_vm(6, "600160005401600055", "");
    EXPECT_EQ(r.status_code, EVMC_SUCCESS);
    EXPECT_EQ(r.gas_left, 0);
    EXPECT_EQ(r, ExpectedOutput(""));
    EXPECT_EQ(storage_value,
              0x00000000000000000000000000000000000000000000000000000000000000bc_bytes32);
}
