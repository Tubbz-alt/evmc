// EVMC: Ethereum Client-VM Connector API.
// Copyright 2016-2020 The EVMC Authors.
// Licensed under the Apache License, Version 2.0.

/// @file
/// Example implementation of the EVMC VM interface.
///
/// This VM does not do anything useful except for showing how EVMC VM API
/// should be implemented. The implementation uses the C API directly
/// and is done in simple C++ for readability.

#include "example_vm.h"
#include <evmc/evmc.h>
#include <evmc/helpers.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>


/// The example VM instance struct extending the evmc_vm.
struct example_vm : evmc_vm
{
    int verbose = 0;  ///< The verbosity level.
    example_vm();     ///< Constructor to initialize the evmc_vm struct.
};

/// The implementation of the evmc_vm::destroy() method.
static void destroy(evmc_vm* vm)
{
    delete static_cast<example_vm*>(vm);
}

/// The example implementation of the evmc_vm::get_capabilities() method.
static evmc_capabilities_flagset get_capabilities(evmc_vm* /*instance*/)
{
    return EVMC_CAPABILITY_EVM1;
}

/// Example VM options.
///
/// The implementation of the evmc_vm::set_option() method.
/// VMs are allowed to omit this method implementation.
static enum evmc_set_option_result set_option(evmc_vm* instance,
                                              const char* name,
                                              const char* value)
{
    example_vm* vm = static_cast<example_vm*>(instance);
    if (std::strcmp(name, "verbose") == 0)
    {
        if (value == nullptr)
            return EVMC_SET_OPTION_INVALID_VALUE;

        char* end = nullptr;
        long int v = std::strtol(value, &end, 0);
        if (end == value)  // Parsing the value failed.
            return EVMC_SET_OPTION_INVALID_VALUE;
        if (v > 9 || v < -1)  // Not in the valid range.
            return EVMC_SET_OPTION_INVALID_VALUE;
        vm->verbose = (int)v;
        return EVMC_SET_OPTION_SUCCESS;
    }

    return EVMC_SET_OPTION_INVALID_NAME;
}

/// The example implementation of the evmc_vm::execute() method.
static evmc_result execute(evmc_vm* instance,
                           const evmc_host_interface* host,
                           evmc_host_context* context,
                           enum evmc_revision rev,
                           const evmc_message* msg,
                           const uint8_t* code,
                           size_t code_size)
{
    example_vm* vm = static_cast<example_vm*>(instance);

    if (vm->verbose > 0)
        std::puts("execution started\n");

    int64_t gas_left = msg->gas;

    evmc_uint256be stack[1024];
    evmc_uint256be* sp = stack;

    uint8_t memory[1024] = {};

    for (size_t pc = 0; pc < code_size; pc++)
    {
        // Check remaining gas, assume each instruction costs 1.
        gas_left -= 1;
        if (gas_left < 0)
            return evmc_make_result(EVMC_OUT_OF_GAS, 0, nullptr, 0);

        switch (code[pc])
        {
        default:
            return evmc_make_result(EVMC_UNDEFINED_INSTRUCTION, 0, nullptr, 0);

        case 0x00:  // STOP
            return evmc_make_result(EVMC_SUCCESS, gas_left, nullptr, 0);

        case 0x01:  // ADD
        {
            sp--;
            uint8_t a = sp->bytes[31];
            sp--;
            uint8_t b = sp->bytes[31];
            uint8_t sum = static_cast<uint8_t>(a + b);
            evmc_uint256be value = {};
            value.bytes[31] = sum;
            *sp = value;
            sp++;
            break;
        }

        case 0x30:  // ADDRESS
        {
            evmc_address address = msg->destination;
            evmc_uint256be value = {};
            std::memcpy(&value.bytes[12], address.bytes, sizeof(address.bytes));
            *sp = value;
            sp++;
            break;
        }

        case 0x43:  // NUMBER
        {
            evmc_uint256be value = {};
            value.bytes[31] = static_cast<uint8_t>(host->get_tx_context(context).block_number);
            *sp = value;
            sp++;
            break;
        }

        case 0x52:  // MSTORE
        {
            sp--;
            uint8_t index = sp->bytes[31];
            sp--;
            evmc_uint256be value = *sp;
            std::memcpy(&memory[index], value.bytes, sizeof(value.bytes));
            break;
        }

        case 0x54:  // SLOAD
        {
            sp--;
            evmc_uint256be index = *sp;
            evmc_uint256be value = host->get_storage(context, &msg->destination, &index);
            *sp = value;
            sp++;
            break;
        }

        case 0x55:  // SSTORE
        {
            sp--;
            evmc_uint256be index = *sp;
            sp--;
            evmc_uint256be value = *sp;
            host->set_storage(context, &msg->destination, &index, &value);
            break;
        }

        case 0x60:  // PUSH1
        {
            uint8_t byte = code[pc + 1];
            pc++;
            evmc_uint256be value = {};
            value.bytes[31] = byte;
            *sp = value;
            sp++;
            break;
        }

        case 0xf3:  // RETURN
        {
            sp--;
            uint8_t index = sp->bytes[31];
            sp--;
            uint8_t size = sp->bytes[31];
            return evmc_make_result(EVMC_SUCCESS, gas_left, &memory[index], size);
        }

        case 0xfd:  // REVERT
        {
            if (rev < EVMC_BYZANTIUM)
                return evmc_make_result(EVMC_UNDEFINED_INSTRUCTION, 0, nullptr, 0);

            sp--;
            uint8_t index = sp->bytes[31];
            sp--;
            uint8_t size = sp->bytes[31];
            return evmc_make_result(EVMC_REVERT, gas_left, &memory[index], size);
        }
        }
    }

    return evmc_make_result(EVMC_SUCCESS, gas_left, nullptr, 0);
}


/// @cond internal
#if !defined(PROJECT_VERSION)
/// The dummy project version if not provided by the build system.
#define PROJECT_VERSION "0.0.0"
#endif
/// @endcond

example_vm::example_vm()
  : evmc_vm{EVMC_ABI_VERSION, "example_vm",       PROJECT_VERSION, ::destroy,
            ::execute,        ::get_capabilities, ::set_option}
{}

evmc_vm* evmc_create_example_vm()
{
    return new example_vm;
}
