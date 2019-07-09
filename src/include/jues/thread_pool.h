/*
 * Filename:
 * ---------
 * thread_pool.h
 *
 * Project:
 * --------
 * thread_pool
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

#ifndef thread_pool_H
#define thread_pool_H
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









class thread_pool
{
public:
    // task queue type.
    enum TYPE{
        TYPE_FIFO,
        TYPE_FILO,
        TYPE_LEVEL,
    };
    // task info.
    struct task_info{
        std::atomic_bool runing;
        std::thread *thread;
    };
public:
    // task queue type, thread count.
    inline thread_pool( const TYPE &type = TYPE_FIFO, const size_t &count = 1 )
    {
        this->m_runing = false;
        this->m_type = type;
        this->m_count = 0;
        this->m_count_resize = count;
        this->m_task_working_count = 0;
    }
    inline virtual ~thread_pool()
    {
        this->stop();
    }

public:
    // task queue type.
    inline TYPE type() const
    {
        return this->m_type;
    }
    // thread count.
    inline size_t count() const
    {
        return this->m_count;
    }
    // current run task queue count.
    inline size_t working_count() const
    {
        return this->m_task_working_count;
    }
    // current waiting run task queue count.
    inline size_t waiting_count()
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

    // thread pool run state.
    inline bool runing() const
    {
        return this->m_runing;
    }

    // start thread pool admin.
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
        task_info *info;
        for ( size_t i=0; count > i;i++ )
        {
            info = new task_info();
            info->thread = new std::thread(&thread_pool::run,this,info);
            this->m_pool.insert(std::pair<std::thread::id,task_info*>(info->thread->get_id(),info));
            //this->m_pool.emplace_back(&thread_pool::run,this);
        }

        // wait
        while ( this->m_count != this->m_count_resize )
        {
            thread_pool::sleep(5);
        }
        thread_pool::sleep(5);
    }

    // stop thread pool admin.
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
        task_info *info;
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

    // reset thread pool thread count.
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
            task_info *info;
            for ( size_t i=0; count > i;i++ )
            {
                info = new task_info();
                info->thread = new std::thread(&thread_pool::run,this,info);
                this->m_pool.insert(std::pair<std::thread::id,task_info*>(info->thread->get_id(),info));
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
        task_info *info;
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

    // wait all task end.
    inline void wait( const size_t &milliseconds = 100 )
    {
        while ( 0 < this->waiting_count() )
        {
            this->sleep(milliseconds);
        }
    }

    // clear all waiting queue.
    inline void clear()
    {
        std::lock_guard<std::mutex> lock{ this->m_task_list_lock };
        //
        switch (this->m_type)
        {
        case TYPE_FIFO:
            this->m_fifo_tasks = std::queue<task>();
            break;
        case TYPE_FILO:
            this->m_filo_tasks = std::stack<task>();
            break;

        case TYPE_LEVEL:
            this->m_level_tasks.clear();
            break;
        }
    }
public:
    // sleep this thread.
    inline static void sleep( const size_t &milliseconds )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

public:
    // commit one task to queue.
    template<typename _Callable, typename... _Args>
    inline auto commit( _Callable&& __f, _Args&&... __args ) ->std::future<decltype(__f(__args...))>
    {
        return this->commit(0,__f,__args...);
    }

    // commit one have run level task to queue.
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
    using task = std::function<void()>;

    // thread run function.
    inline void run( task_info *info )
    {
        this->m_count++;
        info->runing = true;
        //
        std::mutex task_lock;
        while (true == this->m_runing)
        {
            std::unique_lock<std::mutex> lock(task_lock);
            task task_;
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
                if ( false == this->get_task(&task_) )
                {
                    break;
                }

                this->m_task_working_count++;
                task_();
                this->m_task_working_count--;
            }

        }
        //
        info->runing = false;
        this->m_count--;
    }

    // get task for queue.
    inline bool get_task( task *task_ )
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
            *task_ = std::move(this->m_fifo_tasks.front());
            this->m_fifo_tasks.pop();
            break;
        case TYPE_FILO:
            if ( 0 >= this->m_filo_tasks.size() )
            {
                return false;
            }
            *task_ = std::move(this->m_filo_tasks.top());
            this->m_filo_tasks.pop();
            break;

        case TYPE_LEVEL:
            if ( 0 >= this->m_level_tasks.size() )
            {
                return false;
            }
            std::multimap<int,task>::iterator it = this->m_level_tasks.end();
            it--;
            *task_ = std::move((*it).second);
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
    std::queue<task> m_fifo_tasks;
    std::stack<task> m_filo_tasks;
    std::multimap<int,task> m_level_tasks;

    // lock
    std::mutex m_task_list_lock;
    std::mutex m_task_pool_lock;

    // task
    std::condition_variable m_task_cv;
    std::atomic_size_t m_task_working_count;

    // task admin
    std::map<std::thread::id,task_info*> m_pool;
};







}
#endif // thread_pool_H
