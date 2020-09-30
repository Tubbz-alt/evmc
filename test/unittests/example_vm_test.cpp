// EVMC: Ethereum Client-VM Connector API.
// Copyright 2020 The EVMC Authors.
// Licensed under the Apache License, Version 2.0.

#include "../../examples/example_vm/example_vm.h"
#include <evmc/evmc.hpp>
#include <tools/utils/utils.hpp>
#include <gtest/gtest.h>
#include <cstring>

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

evmc::result execute_in_example_vm(int64_t gas,
                                   const uint8_t* code,
                                   size_t code_size,
                                   const uint8_t* input,
                                   size_t input_size) noexcept
{
    using namespace evmc::literals;

    evmc_message msg{};
    msg.gas = gas;
    msg.sender = 0x0000000000000000000000000000000000000005_address;
    msg.destination = 0x000000000000000000000000000000000000000d_address;
    msg.input_data = input;
    msg.input_size = input_size;

    return vm.execute(EVMC_FRONTIER, msg, code, code_size);
}

evmc::result execute_in_example_vm(int64_t gas,
                                   const char* code_hex,
                                   const char* input_hex = "") noexcept
{
    const auto code = evmc::from_hex(code_hex);
    const auto input = evmc::from_hex(input_hex);
    return execute_in_example_vm(gas, code.data(), code.size(), input.data(), input.size());
}
}  // namespace

TEST(example_vm, return_address)
{
    // Assembly: `{ mstore(0, address()) return(12, 20) }`.
    const auto r = execute_in_example_vm(6, "306000526014600cf3", "");
    EXPECT_EQ(r.status_code, EVMC_SUCCESS);
    EXPECT_EQ(r.gas_left, 0);
    EXPECT_EQ(r, ExpectedOutput("000000000000000000000000000000000000000d"));
}
