#include <chrono>
#include <memory>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include <mutex>
#include <coroutine>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executor.hpp"
#include "std_srvs/srv/empty.hpp"
#include "std_msgs/msg/string.hpp"
#include "cospike/coroutine.hpp"

#include "cuda_runtime.h"
#include "device_types.h"
#include "exp/exp0_cuda.cuh"

using namespace std::chrono_literals;
using std::placeholders::_1;
#define gettid() syscall(__NR_gettid)
#define USE_INTRA_PROCESS_COMMS true 
#define DUMMY_LOAD_ITER	1000
#define THREAD_SIZE     3

timeval starting_time;
int dummy_load_calib = 1;

void dummy_load(int load_ms, const char * name_str) {
    int i, j;
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    rclcpp::sleep_for(100ms);
}

int dummy_load_sleep(int load_ms, const char * name_str) {
    int i, j;
    timeval ftime, ctime;
    // Do sth.
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    
    gettimeofday(&ftime, NULL);
    int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
    long tv_sec = duration_us / 1000000;
    long tv_usec = duration_us - tv_sec * 1000000;

    // Wait for the machine
    rclcpp::sleep_for(450ms);

    gettimeofday(&ctime, NULL);
    duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
    tv_sec = duration_us / 1000000;
    tv_usec = duration_us - tv_sec * 1000000;
    
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    return 1;
}

namespace cb_chain_demo
{
class StartNode : public rclcpp::Node
{
public:
    StartNode(const std::string node_name, const std::string pub_topic, int exe_time, int period, bool end_flag) 
        : Node(node_name, rclcpp::NodeOptions().use_intra_process_comms(USE_INTRA_PROCESS_COMMS)), count_(0), exe_time_(exe_time), period_(period), end_flag_(end_flag)
    {
        publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        name_ = node_name;
        timer_ = this->create_wall_timer(600ms, std::bind(&StartNode::timer_callback, this));
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
private:
    std::string name_;
    size_t count_;
    int exe_time_;
    int period_;
    timeval ctime, ftime, create_timer, latency_time;
    bool end_flag_;

    void show_time(timeval ftime, timeval ctime) 
    {
        int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
        long tv_sec = duration_us / 1000000;
        long tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

        duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    }

    void timer_callback()
    {
        gettimeofday(&ftime, NULL);

        dummy_load(100, this->name_.c_str());

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = std::to_string(count_++);

        gettimeofday(&ctime, NULL);
        
        publisher_->publish(message);
        show_time(ftime, ctime);
    }
};

class IntermediateNodeCPU : public rclcpp::Node
{
public:
    IntermediateNodeCPU(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        // create_subscription interface for sync callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 1, std::bind(&IntermediateNodeCPU::callback, this, _1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        this->name_ = node_name;
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
private:
    std::string name_;
    size_t count_;
    int exe_time_;
    timeval ctime, ftime;
    double latency;
    bool end_flag_;

    void show_time(timeval ftime, timeval ctime) 
    {
        int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
        long tv_sec = duration_us / 1000000;
        long tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

        duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    }

    int callback(const std_msgs::msg::String::SharedPtr msg) {

        gettimeofday(&ftime, NULL);
        /* Non-Blocking Style */
        int duration_us;
        long tv_sec, tv_usec;

        //^^^^^ CPU Run It
        int N = 1000000;
        int* cpu_a = new int[N];
        for (int i = 0; i < N; i++) {
            cpu_a[i] = i;
        }
        for (size_t idx = 0; idx < N; idx++)
        {
            int item = cpu_a[idx];
            for (int i = 0; i < 100; i++) {
                cpu_a[idx] += item;
                // ^^^^^ Extra code to evaluate time
                if (idx > 0) {
                    cpu_a[idx] += cpu_a[idx - 1];
                }
            }
            cpu_a[idx] += 2;
        }
        gettimeofday(&ctime, NULL);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;
        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }
};

class IntermediateNodeGPU : public rclcpp::Node
{
public:
    IntermediateNodeGPU(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        // create_subscription interface for async callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(true, sub_topic, 10, std::bind(&IntermediateNodeGPU::co_callback, this, _1));
        // subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 10, std::bind(&IntermediateNodeGPU::callback, this, _1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        this->name_ = node_name;

        this->gpu_record_ = 0;
        this->gpu_await_time_us_ = 0;
        this->gpu_base_latency_us_ = 0;
        this->gpu_best_await_time_us_ = 0;
        this->gpu_best_eval_num_ = -100000;

        srand((unsigned)time(NULL));
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;

private:
    
    timeval ctime, ftime;
    std::string name_;
    size_t count_;
    int exe_time_;
    double latency;
    bool end_flag_;

    int gpu_record_;
    long gpu_await_time_us_;
    long gpu_base_latency_us_;
    long gpu_best_await_time_us_;
    long gpu_best_eval_num_;

    void show_time(timeval ftime, timeval ctime, int mark = false) 
    {
        int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
        long tv_sec = duration_us / 1000000;
        long tv_usec = duration_us - tv_sec * 1000000;
        if (mark) RCLCPP_INFO(this->get_logger(), "[*] [PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
        else RCLCPP_INFO(this->get_logger(), "[PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

        duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        if (mark) RCLCPP_INFO(this->get_logger(), "[*] [PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
        else RCLCPP_INFO(this->get_logger(), "[PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    }

    int callback(const std_msgs::msg::String::SharedPtr msg) {

        gettimeofday(&ftime, NULL);
        int N = 200000000;
        int duration_us;
        long tv_sec, tv_usec;

        //^^^^^ GPU Run It
        int* h_a = new int[N];
        for (int i = 0; i < N; i++) {
            h_a[i] = i + this->gpu_await_time_us_;
        }

        int* d_a; int device = 5;
        cudaSetDevice(device);
        cudaMalloc(&d_a, N * sizeof(int));
        cudaMemcpy(d_a, h_a, N * sizeof(int), cudaMemcpyHostToDevice);

        int threadsPerBlock = 256;
        int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

        cudaStream_t stream1;
        cudaStreamCreate(&stream1);
        kernelCpp(h_a, N, threadsPerBlock, blocksPerGrid, d_a, stream1);
        gettimeofday(&ctime, NULL);
        show_time(ftime, ctime); // First Show Time

        cudaStreamSynchronize(stream1);

        gettimeofday(&ftime, NULL);
        cudaMemcpyAsync(h_a, d_a, N * sizeof(int), cudaMemcpyDeviceToHost, stream1);
        cudaStreamDestroy(stream1);
        delete[] h_a;
        cudaFree(d_a);

        this->gpu_record_ += 1;
        gettimeofday(&ctime, NULL);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;
        if (publisher_) publisher_->publish(message);
        
        show_time(ftime, ctime, true);

        return 1;
    }

    long eval(long base_latency, long cur_latency, long sync_time){
        // The larger the better
        return (base_latency - cur_latency) - sync_time;
    }

    Task<int, RosCoExecutor> co_callback(const std_msgs::msg::String::SharedPtr msg) {
        
        gettimeofday(&ftime, NULL);
        int N = 200000000;
        int duration_us;
        long tv_sec, tv_usec;

        //^^^^^ GPU Run It
        int* h_a = new int[N];
        for (int i = 0; i < N; i++) {
            h_a[i] = i + this->gpu_await_time_us_;
        }

        int* d_a; int device = 5;
        cudaSetDevice(device);
        cudaMalloc(&d_a, N * sizeof(int));
        cudaMemcpy(d_a, h_a, N * sizeof(int), cudaMemcpyHostToDevice);

        int threadsPerBlock = 256;
        int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

        cudaStream_t stream1;
        cudaStreamCreate(&stream1);

        timeval kerftime, kerctime;
        timeval allftime;
        gettimeofday(&allftime, NULL);
        kernelCpp(h_a, N, threadsPerBlock, blocksPerGrid, d_a, stream1);

        if (this->gpu_record_ < 4) {
            // Record Phase
            gettimeofday(&kerftime, NULL);
            gettimeofday(&ctime, NULL);
            RCLCPP_INFO(this->get_logger(), "[Round: %ld] [PID: %ld] Before First Show Time",this->gpu_record_, gettid());
            show_time(ftime, ctime);

            cudaStreamSynchronize(stream1);

            gettimeofday(&ftime, NULL);
            gettimeofday(&kerctime, NULL);
            duration_us = (kerctime.tv_sec  - kerftime.tv_sec ) * 1000000 + 
                          (kerctime.tv_usec - kerftime.tv_usec);
            long all_duration_us = (kerctime.tv_sec - allftime.tv_sec) * 1000000 + (kerctime.tv_usec - allftime.tv_usec);
            
            if (gpu_record_ == 3) {
                this->gpu_await_time_us_ = (this->gpu_await_time_us_ + duration_us) >> 2;
                this->gpu_base_latency_us_ = (this->gpu_base_latency_us_ + all_duration_us) >> 2;
            } else {
                this->gpu_await_time_us_ += duration_us;
                this->gpu_base_latency_us_ += all_duration_us;
            }
            
            // Time Report
            RCLCPP_INFO(this->get_logger(), "[Round: %d] All [us: %ld] Await [us: %ld] Sync [us: %ld]", 
                        this->gpu_record_, all_duration_us, this->gpu_await_time_us_, duration_us);

        } else {
            // Pending Phase
            gettimeofday(&ctime, NULL);
            RCLCPP_INFO(this->get_logger(), "[Round: %ld] [PID: %ld] Before First Show Time",this->gpu_record_, gettid());
            show_time(ftime, ctime);

            co_await std::chrono::duration<int, std::chrono::microseconds::period>(this->gpu_await_time_us_);

            gettimeofday(&ftime, NULL);

            gettimeofday(&kerftime, NULL);
            cudaStreamSynchronize(stream1);
            gettimeofday(&kerctime, NULL);
            
            duration_us = (kerctime.tv_sec - kerftime.tv_sec) * 1000000 + (kerctime.tv_usec - kerftime.tv_usec);
            tv_sec = duration_us / 1000000;
            tv_usec = duration_us - tv_sec * 1000000;

            long all_duration_us = (kerctime.tv_sec - allftime.tv_sec) * 1000000 + (kerctime.tv_usec - allftime.tv_usec);

            // Time Report
            RCLCPP_INFO(this->get_logger(), "[Round: %ld] [Base: %ld] [Score: %ld] All [us: %ld] Await [us: %ld] Sync [us: %ld]",
                        this->gpu_record_, this->gpu_base_latency_us_, 
                        eval(this->gpu_base_latency_us_, all_duration_us, duration_us),
                        all_duration_us, this->gpu_await_time_us_, duration_us);
            
            RCLCPP_INFO(this->get_logger(), "[Round: %ld] [Best Score: %ld] Await [us: %ld]",
                        this->gpu_record_, this->gpu_best_eval_num_, this->gpu_best_await_time_us_);
            
            // Auto Modification
            long delta = 10;
            if (duration_us > (this->gpu_base_latency_us_ >> 4) && this->gpu_await_time_us_ + (duration_us >> 1) < this->gpu_base_latency_us_) 
                this->gpu_await_time_us_ += duration_us >> 1;
            else if (duration_us < 20) 
                this->gpu_await_time_us_  = this->gpu_await_time_us_ - (this->gpu_await_time_us_ >> 3);
            else {
                if (eval(this->gpu_base_latency_us_, all_duration_us, duration_us) > this->gpu_best_eval_num_) {
                    this->gpu_best_await_time_us_ = this->gpu_await_time_us_;
                    this->gpu_best_eval_num_ = eval(this->gpu_base_latency_us_, all_duration_us, duration_us);
                    if (duration_us > (this->gpu_base_latency_us_ >> 8)) this->gpu_await_time_us_ += (rand() % (delta));
                } else {
                    this->gpu_await_time_us_ = this->gpu_best_await_time_us_ + (rand() % (delta));
                }
            }
        }
        cudaMemcpyAsync(h_a, d_a, N * sizeof(int), cudaMemcpyDeviceToHost, stream1);
        cudaStreamDestroy(stream1);
        delete[] h_a;
        cudaFree(d_a);

        this->gpu_record_ += 1;
        gettimeofday(&ctime, NULL);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;
        if (publisher_) publisher_->publish(message);
        
        show_time(ftime, ctime, true);
        co_return 1;
    }
};

}

void run_exe(rclcpp::executors::ExecutorNodelet* exe) {
    exe->spin();
}

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // Define graph: c1_timer -> c1_callback
    // std::make_shared --> return the ptr & allocate the memory space on heap
    auto c1_timer = std::make_shared<cb_chain_demo::StartNode>("Timer_callback1", "c1", 100, 2000, false);
    auto c1_r_cb1 = std::make_shared<cb_chain_demo::IntermediateNodeGPU>("Regular_callback11", "c1", "", 100, true);
    auto c1_r_cb2 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback12", "c1", "", 100, true);
    auto c1_r_cb3 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback13", "c1", "", 100, true);
    auto c1_r_cb4 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback14", "c1", "", 100, true);
    auto c1_r_cb5 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback15", "c1", "", 100, true);
    auto c1_r_cb6 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback16", "c1", "", 100, true);
    auto c1_r_cb7 = std::make_shared<cb_chain_demo::IntermediateNodeCPU>("Regular_callback17", "c1", "", 100, true);
    
    // Create executors
    int number_of_threads = 5;
    rclcpp::executors::ExecutorNodelet exec1(rclcpp::executor::ExecutorArgs(), number_of_threads, true);
    // rclcpp::executors::ExecutorNodelet exec2(rclcpp::executor::ExecutorArgs(), number_of_threads - 3, true);
    
    std::queue<Task<int, RosCoExecutor> > task_queue;
    
    // Allocate callbacks to executors
    exec1.add_node(c1_timer);
    exec1.add_node(c1_r_cb1);
    exec1.add_node(c1_r_cb2);
    exec1.add_node(c1_r_cb3);
    exec1.add_node(c1_r_cb4);
    exec1.add_node(c1_r_cb5);
    exec1.add_node(c1_r_cb6);
    exec1.add_node(c1_r_cb7);

    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    //^^^^ Spin lock: Single-Executor Mode
    exec1.spin();

    //^^^^ Spin lock: Multi-Executor Mode
    // auto th2 = std::thread(run_exe, &exec2);
    // run_exe(&exec1);
    // th2.join();

    // Remove Extra-node
    exec1.remove_node(c1_timer);
    exec1.remove_node(c1_r_cb1);
    exec1.remove_node(c1_r_cb2);
    exec1.remove_node(c1_r_cb3);
    exec1.remove_node(c1_r_cb4);
    exec1.remove_node(c1_r_cb5);
    exec1.remove_node(c1_r_cb6);
    exec1.remove_node(c1_r_cb7);

    // Shutdown
    rclcpp::shutdown();
    return 0;
}