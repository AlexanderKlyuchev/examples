#include "common/ThreadPool.h"
#include "logger.h"

static void print_log(...) {}

template<typename T, typename... Args>
void print_log(const char *s, const T& value, const Args&... args)
{
    while (*s) {
        if (*s == '%') {
            if (*(s + 1) == '%') {
                ++s;
            }else {
                sstl::sstl_log_message(sstl::Info, value,false);
                //std::cout<<value;
                print_log(s + 1, args...); // call even when *s == 0 to detect extra arguments
                return;
            }
        }
        sq::logger::log_message(sq::logger::Info,s++,false);
    }
    throw std::runtime_error("extra arguments provided to printf");
}

namespace sq { namespace common {


inline void usleep(unsigned int usecs)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usecs));
}
inline std::thread::id get_thread_id()
{
    return std::this_thread::get_id();
}

ThreadPool::ThreadPool(size_t max_threads) : m_max_threads(max_threads), m_terminating(false),
                                               m_inprogress(0) {
    m_threads.reserve(max_threads);
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::process_completed_tasks() {
    task::list completed;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_completed.swap(completed);
    }
    for (auto it = completed.begin(); it != completed.end(); ++it) {
        (*it)->on_post_execute();
        it->reset();
    }
}

void ThreadPool::add_task(const task::sptr &task) {
    if (!task) return;
    if (m_terminating) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.push_back(task);
        if (m_threads.size() < m_max_threads)
        {
            spawn();
        }
    }
    m_cond.notify_one();
}

void ThreadPool::stop() {
    print_log("[sstl_ThreadPool (%)] stop begin", this);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending.clear();
        m_terminating = true;
    }

    print_log("[sstl_ThreadPool (%)] cancel tasks ok", this);
    print_log("[sstl_ThreadPool (%)] count threads %", this, m_threads.size());

    m_cond.notify_all();

    print_log("[sstl_ThreadPool (%)] begin deletions", this);

    for (const auto &tr : m_threads) {
        tr->join();
        print_log("[sstl_ThreadPool (%)]  thread finished", this);
    }
    m_threads.clear();
    //m_completed.clear();

    print_log("[sstl_ThreadPool (%)] stop end", this);
}


void ThreadPool::spawn() {
    std::unique_ptr<std::thread> tr_ptr = std::unique_ptr<std::thread>(
            new std::thread(bind(&ThreadPool::thread_func, this)));
    m_threads.push_back(move(tr_ptr));
}

size_t ThreadPool::current_threads() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_threads.size();
}

size_t ThreadPool::working_tasks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t res = 0;
    res = m_pending.size() + m_inprogress;
    return res;
}

size_t ThreadPool::all_tasks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t res = 0;
    res = m_pending.size() + m_inprogress + m_completed.size();
    return res;
}

void ThreadPool::thread_func(ThreadPool *_this) {
    while (true) {
        /// try quit
        if (_this->m_terminating) break;

        task::sptr task_;
        /// extract task
        {
            /// manual acquire/release with non-scope logick
            std::unique_lock<std::mutex> lock(_this->m_mutex);
            /// wait new task
            if (_this->m_pending.empty()) {
                _this->m_cond.wait(lock);
            }

            if (_this->m_terminating) {
                break;
            }

            if (_this->m_pending.empty()) {
                continue;
            }

            task_ = _this->m_pending.front();
            _this->m_pending.pop_front();
            ++_this->m_inprogress;
        }

        task_->do_in_background();

        /// store task
        {
            std::lock_guard<std::mutex> lock(_this->m_mutex);
            --_this->m_inprogress;
            _this->m_completed.push_back(task_);
        }

    }
}
}}



