/*
 * Filename:
 * ---------
 * main.cpp
 *
 * Project:
 * --------
 * thread_pool
 *
 * Description:
 * ------------
 *
 * C++ thread pool demo.
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

#include <iostream>
#include <jues/thread_pool.h>

using namespace std;


void test1()
{
    cout<<"test1: tid="<<this_thread::get_id()<<endl;
}

int test2( int a, int b )
{
    cout<<"test2: tid="<<this_thread::get_id()<<", a="<<a<<", b="<<b<<endl;
    //
    return a+b;
}

void test3( const size_t milliseconds )
{
    cout<<"test3: tid="<<this_thread::get_id()<<" sleep begin."<<endl;
    jues::thread_pool::sleep(milliseconds);
    cout<<"test3: tid="<<this_thread::get_id()<<" sleep end."<<endl;
}




//
void thread_fifo_pool()
{
    jues::thread_pool fifo_pool( jues::thread_pool::TYPE_FIFO,1 );
    //
    fifo_pool.start();
    fifo_pool.commit(test1);
    fifo_pool.commit(test2,1,2);
    fifo_pool.commit(test3,1000);
    //
    fifo_pool.wait();
}

//
void thread_filo_pool()
{
    jues::thread_pool filo_pool( jues::thread_pool::TYPE_FILO,1 );
    //
    filo_pool.start();
    filo_pool.commit(test1);
    filo_pool.commit(test2,1,2);
    filo_pool.commit(test3,1000);
    //
    filo_pool.wait();
}

//
void thread_level_pool()
{
    jues::thread_pool level_pool( jues::thread_pool::TYPE_LEVEL,1 );
    //
    level_pool.start();
    level_pool.commit(-1,test1);
    level_pool.commit(10,test2,1,2);
    level_pool.commit(100,test3,1000);
    //
    level_pool.wait();
}


//
void thread_resize_pool()
{
    jues::thread_pool fifo_pool( jues::thread_pool::TYPE_FIFO,2 );
    //
    fifo_pool.start();
    for ( int i =0;10 >i;i++ )
    {
        fifo_pool.commit(test3,1000);
    }
    fifo_pool.sleep(1000);
    fifo_pool.resize(1);

    //
    fifo_pool.wait();
}


//
class demo
{
public:
    demo() {}

    void run( const size_t milliseconds )
    {
        cout<<"demo::run: tid="<<this_thread::get_id()<<" sleep begin."<<endl;
        jues::thread_pool::sleep(milliseconds);
        cout<<"demo::run: tid="<<this_thread::get_id()<<" sleep end."<<endl;
    }
};
//
void thread_class_function_pool()
{
    jues::thread_pool level_pool( jues::thread_pool::TYPE_LEVEL,1 );
    demo d;
    //
    level_pool.start();
    level_pool.commit(10,std::bind(&demo::run,&d,1000));
    //
    level_pool.wait();
}

int main()
{
    cout<<"main: tid="<<this_thread::get_id()<<", thread_fifo_pool()----------------"<<endl;
    thread_fifo_pool();

    cout<<"main: tid="<<this_thread::get_id()<<", thread_filo_pool()----------------"<<endl;
    thread_filo_pool();

    cout<<"main: tid="<<this_thread::get_id()<<", thread_level_pool()---------------"<<endl;
    thread_level_pool();

    cout<<"main: tid="<<this_thread::get_id()<<", thread_resize_pool()---------------"<<endl;
    thread_resize_pool();

    cout<<"main: tid="<<this_thread::get_id()<<", thread_class_function_pool()---------------"<<endl;
    thread_class_function_pool();

    //
    cin.get();
    return 0;
}
