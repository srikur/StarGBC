#include <atomic>
#include <doctest/doctest.h>
#include <latch>
#include "ThreadContext.h"

TEST_CASE("diagnostics: selected work runs concurrently within the worker limit") {
    TestWorkers workers(2);
    std::latch simultaneous(2);
    std::atomic<unsigned> active{};
    std::atomic<unsigned> peak{};
    std::vector<std::shared_future<unsigned>> futures;
    for (unsigned i = 0; i < 12; ++i) {
        futures.push_back(workers.Submit([&, i] {
            peak.fetch_max(++active);
            if (i < 2) {
                simultaneous.count_down();
                simultaneous.wait();
            }
            --active;
            return i;
        }));
    }
    for (unsigned i = 0; i < futures.size(); ++i)
        CHECK(futures[i].get() == i);
    CHECK(peak == 2);
    auto exception = workers.Submit([] { throw std::runtime_error("worker failure"); });
    CHECK_THROWS_AS(exception.get(), std::runtime_error);
}
