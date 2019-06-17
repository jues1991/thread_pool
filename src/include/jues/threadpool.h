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
    inline ThreadPool( const TYPE &type = TYPE_FIFO, const size_t &count = 1 )
    {
        this->m_runing = false;
        this->m_type = type;
        this->m_count = 0;
        this->m_count_resize = count;
        this->m_task_working_count = 0;
    }
    inline virtual ~ThreadPool()
    {
        this->stop();
    }

public:
    inline TYPE type() const
    {
        return this->m_type;
    }
    inline size_t count() const
    {
        return this->m_count;
    }

    inline size_t workngCount() const
    {
        return this->m_task_working_count;
    }
    inline size_t waitingCount()
    {
        std::lock_guard<std::mutex> lock(this->m_task_list_lock);
        size_t count = 0;
        //
        switch (this->m_type)
        {
        case TYPE_FIFO:
            count = this->m_fifo_tasks.size();
            break;
        case TYPE_FILO:
            count = this->m_filo_tasks.size();
            break;

        case TYPE_LEVEL:
            count = this->m_level_tasks.size();
            break;
        }
        //
        return count;
    }

    inline bool runing() const
    {
        return this->m_runing;
    }
    inline void start()
    {
        std::lock_guard<std::mutex> lock{ this->m_task_pool_lock };

        if ( true == this->m_runing || 0 >= this->m_count_resize || 0 < this->m_pool.size() )
        {
            return;
        }

        //
        this->m_runing = true;
        const size_t count = this->m_count_resize;
        TaskInfo *info;
        for ( size_t i=0; count > i;i++ )
        {
            info = new TaskInfo();
            info->thread = new std::thread(&ThreadPool::run,this,info);
            this->m_pool.insert(std::pair<std::thread::id,TaskInfo*>(info->thread->get_id(),info));
            //this->m_pool.emplace_back(&ThreadPool::run,this);
        }

        // wait
        while ( this->m_count != this->m_count_resize )
        {
            ThreadPool::sleep(5);
        }
        ThreadPool::sleep(5);
    }
    inline void stop()
    {
        std::lock_guard<std::mutex> lock{ this->m_task_pool_lock };

        if ( false == this->m_runing )
        {
            return;
        }
        //
        this->m_runing = false;
        this->m_task_cv.notify_all();
        TaskInfo *info;
        for (auto it : this->m_pool)
        {
            info = it.second;
            if(info->thread->joinable())
            {
                info->thread->join();
            }
            //
            delete info->thread;
            delete info;
        }
    }
    inline bool resize( const size_t &count )
    {
        std::lock_guard<std::mutex> lock{ this->m_task_pool_lock };
        if ( 0 >= count )
        {
            return false;
        }
        this->m_count_resize = count;
        if ( this->m_count < this->m_count_resize )
        {
            // add task
            const size_t count = this->m_count_resize - this->m_count;
            TaskInfo *info;
            for ( size_t i=0; count > i;i++ )
            {
                info = new TaskInfo();
                info->thread = new std::thread(&ThreadPool::run,this,info);
                this->m_pool.insert(std::pair<std::thread::id,TaskInfo*>(info->thread->get_id(),info));
            }
        }

        // wait
        this->m_task_cv.notify_all();
        while ( this->m_count != this->m_count_resize )
        {
            this->sleep(100);
        }

        // get need clear list
        std::vector<std::thread::id> clear_list;
        TaskInfo *info;
        for ( auto it : this->m_pool )
        {
            info = it.second;
            if ( false == info->runing )
            {
                clear_list.push_back(it.first);
            }
        }

        // clear
        for ( auto tid : clear_list )
        {
            auto it = this->m_pool.find(tid);
            info = it->second;
            //
            if ( true == info->thread->joinable() )
            {
                info->thread->join();
            }
            delete info->thread;
            delete info;
            this->m_pool.erase(it);
        }

        //
        this->m_task_cv.notify_all();
        //
        return true;
    }
    inline void wait()
    {
        while ( 0 < this->waitingCount() )
        {
            this->sleep(100);
        }
    }
public:
    inline static void sleep( const size_t &milliseconds )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

public:
    template<typename _Callable, typename... _Args>
    inline auto commit( _Callable&& __f, _Args&&... __args ) ->std::future<decltype(__f(__args...))>
    {
        return this->commit(0,__f,__args...);
    }
    template<typename _Callable, typename... _Args>
    inline auto commit( const int &level, _Callable&& __f, _Args&&... __args ) ->std::future<decltype(__f(__args...))>
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

    inline void run( TaskInfo *info )
    {
        this->m_count++;
        info->runing = true;
        //
        std::mutex task_lock;
        while (true == this->m_runing)
        {
            std::unique_lock<std::mutex> lock(task_lock);
            Task task;
            //
            if ( this->m_count > this->m_count_resize )
            {
                break;
            }
            this->m_task_cv.wait(lock);
            while ( true == this->m_runing )
            {
                // check count
                if ( this->m_count > this->m_count_resize )
                {
                    break;
                }

                // check task
                if ( true == this->getTask(&task) )
                {
                    this->m_task_working_count++;
                    task();
                    this->m_task_working_count--;
                }
            }

        }
        //
        info->runing = false;
        this->m_count--;
    }

    inline bool getTask( Task *task )
    {
        std::lock_guard<std::mutex> lock{ this->m_task_list_lock };
        //
        switch (this->m_type)
        {
        case TYPE_FIFO:
            if ( 0 >= this->m_fifo_tasks.size() )
            {
                return false;
            }
            *task = std::move(this->m_fifo_tasks.front());
            this->m_fifo_tasks.pop();
            break;
        case TYPE_FILO:
            if ( 0 >= this->m_filo_tasks.size() )
            {
                return false;
            }
            *task = std::move(this->m_filo_tasks.top());
            this->m_filo_tasks.pop();
            break;

        case TYPE_LEVEL:
            if ( 0 >= this->m_level_tasks.size() )
            {
                return false;
            }
            std::multimap<int,Task>::iterator it = this->m_level_tasks.end();
            it--;
            *task = std::move((*it).second);
            this->m_level_tasks.erase(it);
            break;
        }

        //
        return true;
    }



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
