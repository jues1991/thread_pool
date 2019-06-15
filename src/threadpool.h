/*
 * Filename:
 * ---------
 * threadpool.h
 *
 * Project:
 * --------
 * ThreadPool
 *
 * Description:
 * ------------
 *
 * C++ thread pool.
 *
 * Author:
 * ------------
 *
 * jues
 *
 * Email:
 * ------------
 *
 * jues1991@163.com
 *
 * Home:
 * ------------
 *
 * http://note.jues.org.cn/node/357
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <future>
#include <functional>
#include <queue>
#include <stack>
#include <map>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>


namespace jues
{









class ThreadPool
{
public:
    enum TYPE{
        TYPE_FIFO,
        TYPE_FILO,
        TYPE_LEVEL,
    };
    struct TaskInfo{
        std::atomic_bool runing;
        std::thread *thread;
    };
public:
    ThreadPool( const TYPE &type = TYPE_FIFO, const size_t &count = 1 );
    virtual ~ThreadPool();

public:
    TYPE type() const;
    size_t count() const;

    size_t workngCount() const;
    size_t waitingCount();

    bool runing() const;
    void start();
    void stop();
    bool resize( const size_t &count );
    void wait();
public:
    static void sleep( const size_t &milliseconds );
public:
    template<typename _Callable, typename... _Args>
    auto commit( _Callable&& __f, _Args&&... __args ) ->std::future<decltype(__f(__args...))>
    {
        return this->commit(0,__f,__args...);
    }
    template<typename _Callable, typename... _Args>
    auto commit( const int &level, _Callable&& __f, _Args&&... __args ) ->std::future<decltype(__f(__args...))>
    {
        using RetType = decltype(__f(__args...));
        auto task = std::make_shared<std::packaged_task<RetType()> >(
                    std::bind(std::forward<_Callable>(__f), std::forward<_Args>(__args)...)
                    );
        std::future<RetType> future = task->get_future();

        if ( false == this->m_runing )
        {
            return future;
        }

        // add to task list
        {
            std::lock_guard<std::mutex> lock{ this->m_task_list_lock };
            switch (this->m_type)
            {
            case TYPE_FIFO:
                this->m_fifo_tasks.emplace([task](){ (*task)(); });
                break;
            case TYPE_FILO:
                this->m_filo_tasks.emplace([task](){ (*task)(); });
                break;
            case TYPE_LEVEL:
                this->m_level_tasks.emplace(level,[task](){ (*task)(); });
                break;
            }
        }

        // active one task
        this->m_task_cv.notify_one();

        //
        return future;
    }


protected:
    using Task = std::function<void()>;

    void run( TaskInfo *info );

    bool getTask( Task *task );
protected:
    // type
    TYPE m_type;
    std::atomic_size_t m_count;
    std::atomic_size_t m_count_resize;

    std::atomic_bool m_runing;

    // task list
    std::queue<Task> m_fifo_tasks;
    std::stack<Task> m_filo_tasks;
    std::multimap<int,Task> m_level_tasks;

    // lock
    std::mutex m_task_list_lock;
    std::mutex m_task_pool_lock;

    // task
    std::condition_variable m_task_cv;
    std::atomic_size_t m_task_working_count;

    // task admin
    std::map<std::thread::id,TaskInfo*> m_pool;
};







}
#endif // THREADPOOL_H
