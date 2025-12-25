#ifndef STARGBC_THREADCONTEXT_H
#define STARGBC_THREADCONTEXT_H

#include <semaphore>
#include <thread>

static size_t maxThreads = std::thread::hardware_concurrency();
static std::shared_ptr<std::counting_semaphore<> > threadSemaphore;


struct ThreadPermit {
    ThreadPermit() { threadSemaphore->acquire(); }
    ~ThreadPermit() { threadSemaphore->release(); }
};

#endif //STARGBC_THREADCONTEXT_H
