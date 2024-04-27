#ifndef __COSPIKE_TASK_HPP__
#define __COSPIKE_TASK_HPP__

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
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include "cospike/coexecutor.hpp"
#include "cospike/awaiter.hpp"
#include "cospike/io_utils.hpp"
#include "cospike/result.hpp"

template<typename AwaiterImpl, typename R>
concept AwaiterImplRestriction = std::is_base_of<Awaiter<R>, AwaiterImpl>::value;

/**
 * TaskPromise: take result into consideration
 */
template<typename ResultType, typename ExecutorType>
struct TaskPromise {
    // (MUST) initial_suspend & final_suspend & get_return_object
    // --> standard design
    DispatchAwaiter initial_suspend() { 
        return DispatchAwaiter{&executor}; 
    }
    std::suspend_always final_suspend() noexcept { return {}; }
    Task<ResultType, ExecutorType> get_return_object() {
        return Task{ std::coroutine_handle<TaskPromise>::from_promise(*this) };
    }

    // (MUST) use await_transform to obtain the await <expr>
    // --> specify "TaskAwaiter" to handle --> await <coroutine>
    template<typename _ResultType, typename _ExecutorType>
    TaskAwaiter<_ResultType, _ExecutorType> await_transform(Task<_ResultType, _ExecutorType> &&task) {
        return TaskAwaiter<_ResultType, _ExecutorType>(&executor, std::move(task));
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

    // (MUST) process exception
    void unhandled_exception() {
        std::lock_guard lock(completion_lock);
        result = Result<ResultType>(std::current_exception());
        completion.notify_all();
        notify_callbacks();
    }

    // (MUST) return value for supporting "co_return"
    void return_value(ResultType value) {
        std::unique_lock lock(completion_lock);
        result = Result<ResultType>(std::move(value));
        completion.notify_all();
        lock.unlock();
        notify_callbacks();
    }

    // Pass the private result out
    ResultType get_result() {
        std::unique_lock lock(completion_lock);
        if (!result.has_value()) {
            // blocking for result
            completion.wait(lock);
        }
        // give result or throw on exception
        return result->get_or_throw();
    }

    // Execute the func if value is obtained
    void on_completed(std::function<void(Result<ResultType>)> &&func) {
        std::unique_lock lock(completion_lock);
        if (result.has_value()) {
            auto value = result.value();
            lock.unlock();
            func(value); // --> handle is resumed here, should not do any thing after that
        } else {
            completion_callbacks.push_back(func);
        }
    }

    // init the executor
    void promise_executor_init(void* ros_executer_ptr) {
        executor.executor_init(ros_executer_ptr);
    }

    // Indicate Deconstructor
    ~TaskPromise() {
        // printf("[INFO] [PID: %d] [~TaskPromise]\n", gettid());
    }

private:
    std::optional<Result<ResultType>> result;
    std::mutex completion_lock;
    std::condition_variable completion;
    std::list<std::function<void(Result<ResultType>)>> completion_callbacks;
    ExecutorType executor;
    void notify_callbacks() {
        auto value = result.value();
        for (auto &callback : completion_callbacks) {
            // Hint: NoopExecutor & AsyncExecutor will block here,
            //       in that deadlock may occur if this promise is
            //       deconstructed in the callback!
            callback(value);
        }
        completion_callbacks.clear();
    }
};

/**
 * Task: the general return value type
 */
template<typename ResultType, typename ExecutorType = NoopExecutor>
struct Task {
    using promise_type = TaskPromise<ResultType, ExecutorType>;

    // [sync] Used to obtain results
    ResultType get_result() {
        // --> TaskPromise.get_result();
        return handle.promise().get_result();
    }

    void task_executor_init(void* ros_executer_ptr) {
        handle.promise().promise_executor_init(ros_executer_ptr);
    }

    // [async] Used to obtain results
    Task &then(std::function<void(ResultType)> &&func) {
        handle.promise().on_completed([func](auto result) {
            try {
                func(result.get_or_throw());
            } catch (std::exception &e) {
                // ...
            }
        });
        return *this;
    }

    // Used to obtain exceptions
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

    // Execute anyway
    Task &finally(std::function<void()> &&func) {
        handle.promise().on_completed([func](auto result) { func(); });
        return *this;
    }
    explicit Task(std::coroutine_handle<promise_type> handle) noexcept: handle(handle) {}
    Task(Task &&task) noexcept: handle(std::exchange(task.handle, {})) {}
    Task(Task &) = delete;
    Task &operator=(Task &) = delete;
    ~Task() {
        if (handle) handle.destroy();
    }
private:
    std::coroutine_handle<promise_type> handle;
};
#endif