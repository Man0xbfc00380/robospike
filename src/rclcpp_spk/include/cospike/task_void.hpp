#ifndef __TASK_VOID_HPP__
#define __TASK_VOID_HPP__

#include <concepts> // c++20
#include <coroutine> // c++20
#include <exception>
#include <iostream>
#include <thread>
#include <future>
#include <list>
#include <optional>
#include <functional>
#include <mutex>
#include "cospike/executor.hpp"
#include "cospike/awaiter.hpp"
#include "cospike/io_utils.hpp"
#include "cospike/task.hpp"
#include "cospike/result.hpp"

/**
 * TaskPromise: take result into consideration (for void)
 */
template<typename Executor>
struct TaskPromise<void, Executor> {
    DispatchAwaiter initial_suspend() { return DispatchAwaiter{&executor}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    Task<void, Executor> get_return_object() {
        return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
    }

    // (MUST) use await_transform to obtain the await <expr>
    // --> specify "TaskAwaiter" to handle --> await <coroutine>
    template<typename _ResultType, typename _Executor>
    TaskAwaiter<_ResultType, _Executor> await_transform(Task<_ResultType, _Executor> &&task) {
        return TaskAwaiter<_ResultType, _Executor>(&executor, std::move(task));
    }
    // --> specify "SleepAwaiter" to handle --> await <time>
    template<typename _Rep, typename _Period>
    SleepAwaiter await_transform(std::chrono::duration<_Rep, _Period> &&duration) {
        return SleepAwaiter(&executor, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }
    // --> general awaiter to handle --> await <AwaiterImpl>
    template<typename AwaiterImpl>
    requires AwaiterImplRestriction<AwaiterImpl, typename AwaiterImpl::ResultType>
    AwaiterImpl await_transform(AwaiterImpl awaiter) {
        awaiter.install_executor(&executor);
        return awaiter;
    }

    void get_result() {
        // blocking for result or throw on exception
        std::unique_lock lock(completion_lock);
        if (!result.has_value()) {
            completion.wait(lock);
        }
        result->get_or_throw();
    }

    void unhandled_exception() {
        std::lock_guard lock(completion_lock);
        result = Result<void>(std::current_exception());
        completion.notify_all();
        notify_callbacks();
    }

    void return_void() {
        std::lock_guard lock(completion_lock);
        result = Result<void>();
        completion.notify_all();
        notify_callbacks();
    }

    void on_completed(std::function<void(Result<void>)> &&func) {
        std::unique_lock lock(completion_lock);
        if (result.has_value()) {
            auto value = result.value();
            lock.unlock();
            func(value);
        } else {
            completion_callbacks.push_back(func);
        }
    }

    ~TaskPromise() {
        std::unique_lock lock(completion_lock);
        if (!completion_callbacks.empty()) {
            completion.wait(lock);
        }
    }

private:
    std::optional<Result<void>> result;
    std::mutex completion_lock;
    std::condition_variable completion;
    std::list<std::function<void(Result<void>)>> completion_callbacks;
    Executor executor;
    void notify_callbacks() {
        auto value = result.value();
        for (auto &callback : completion_callbacks) {
            callback(value);
        }
        completion_callbacks.clear();
    }
};

/**
 * Task: the general return value type (for void)
 */
template<typename Executor>
struct Task<void, Executor> {
    using promise_type = TaskPromise<void, Executor>;
    void get_result() {
        handle.promise().get_result();
    }
    Task &then(std::function<void()> &&func) {
        handle.promise().on_completed([func](auto result) {
            try {
                result.get_or_throw();
                func();
            } catch (std::exception &e) {
                // ignore.
            }
        });
        return *this;
    }
    Task &catching(std::function<void(std::exception &)> &&func) {
        handle.promise().on_completed([func](auto result) {
            try {
                result.get_or_throw();
            } catch (std::exception &e) {
                func(e);
            }
        });
        return *this;
    }
    Task &finally(std::function<void()> &&func) {
        handle.promise().on_completed([func](auto result) { func(); });
        return *this;
    }
    explicit Task(std::coroutine_handle<promise_type> handle) noexcept: handle(handle) {}
    Task(Task &&task) noexcept: handle(std::exchange(task.handle, {})) {}
    Task(Task &) = delete;
    Task &operator=(Task &) = delete;
    ~Task() { if (handle) handle.destroy(); }

private:
    std::coroutine_handle<promise_type> handle;
};

#endif // !__TASK_VOID_HPP__