#ifndef __COROUTINE_HPP__
#define __COROUTINE_HPP__

#include <concepts>
#include <coroutine>
#include <exception>
#include <iostream>
#include <thread>
#include "cospike/awaiter.hpp"

/**
 * SimpleResult: an empty promise_type
 *         promise_type indicate the struct is for coroutine
 */
struct SimpleResult {
    struct promise_type {
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        SimpleResult get_return_object() { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

#endif