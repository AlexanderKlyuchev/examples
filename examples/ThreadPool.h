#pragma once

#include "Common.h"
#include <vector>
#include <list>
#include <thread>
#include <condition_variable>

namespace sb { namespace common {

extern inline void usleep(unsigned int usecs);
extern inline std::thread::id get_thread_id();

class ThreadPool : public noncopyable
{
public:
    explicit ThreadPool(size_t threads_count = 2);
    ~ThreadPool();

    void do_in_main_thread();

    struct task : public noncopyable
    {
        using sptr = std::shared_ptr<task>;
        using list = std::list<sptr>;

        virtual void do_in_background() = 0;
        virtual void on_post_execute() {}
        virtual void cancel()          {}
        virtual ~task()                {}
    };

    void   add_task(const task::sptr &task);
    void   stop();
    void   process_completed_tasks();
    size_t current_threads();
    size_t working_tasks();
    size_t all_tasks();

private:
    static void thread_func(ThreadPool *_this);
    void spawn();

    std::vector<std::unique_ptr<std::thread> > m_threads;

    size_t                  m_max_threads;
    task::list              m_pending;
    task::list              m_completed;
    std::mutex              m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool>       m_terminating;
    std::atomic<int>        m_inprogress;
};

}} // namespace

