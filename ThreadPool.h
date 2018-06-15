//
// Created by Rechard Catelemmon on 2018/4/11 0011.
//

#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H


#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <future>
#include <crtdefs.h>


class ThreadPool{
public:
    ThreadPool(size_t threads);
    template<class F,class... Args>
            auto enqueue(F&& f,Args&&... args)
            -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();

private:
    std::vector<std::thread> workers;

    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stoped;
};

template <typename F,typename... Args>
auto ThreadPool::enqueue(F&& f,Args&&... args)
->std::future<typename std::result_of<F(Args...)>::type>{

    using res_type =typename std::result_of<F(Args...)>::type;

    auto task=std::make_shared<std::packaged_task<res_type()>>(
            std::bind(std::forward<F>(f),std::forward<Args>(args)...)
    );

    std::future<res_type> res=task->get_future();
    {
        std::unique_lock<std::mutex> ulk(this->queue_mutex);

        if(stoped){
            throw std::runtime_error("cannot enqueue on stoped ThreadPool!");
        }

        tasks.emplace([task](){(*task)();});
    }

    cv.notify_one();
    return res;
};


inline ThreadPool::ThreadPool(size_t threads):stoped(false){
    for(size_t i=0;i<threads;++i){
        workers.emplace_back([this]{
            for(;;){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> ulk(this->queue_mutex);
                    this->cv.wait(ulk,[this]()->bool{ return this->stoped||!this->tasks.empty();});
                    if(this->stoped&&this->tasks.empty()){
                        return;
                    }
                    task=std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}




inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> ulk(queue_mutex);
        stoped=true;
    }
    cv.notify_all();
    for(std::thread &worker:workers)
        worker.join();
}

#endif //THREADPOOL_THREADPOOL_H
