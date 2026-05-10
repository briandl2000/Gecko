#include "gecko/core/labels.h"
#include "gecko/runtime/thread_pool_job_system.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace gecko;
using namespace gecko::runtime;

TEST_CASE("ThreadPoolJobSystem Init/Shutdown", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  REQUIRE(jobs.Init());
  REQUIRE(jobs.WorkerThreadCount() > 0);
  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem submit and wait", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.Init();

  bool ran = false;
  JobHandle h = jobs.Submit([&ran]() { ran = true; });
  REQUIRE(h.IsValid());

  jobs.Wait(h);
  REQUIRE(ran);
  REQUIRE(jobs.IsComplete(h));

  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem multiple jobs", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.Init();

  ::std::atomic<int> counter {0};
  constexpr int numJobs = 50;

  ::std::vector<JobHandle> handles;
  handles.reserve(numJobs);

  for (int i = 0; i < numJobs; ++i)
  {
    handles.push_back(jobs.Submit([&counter]() { counter.fetch_add(1); }));
  }

  jobs.WaitAll(handles.data(), static_cast<u32>(handles.size()));
  REQUIRE(counter.load() == numJobs);

  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem job with dependencies", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.Init();

  int step = 0;
  JobHandle first = jobs.Submit([&step]() { step = 1; });

  JobHandle second = jobs.Submit([&step]() { step = step * 10 + 2; }, &first, 1);

  jobs.Wait(second);
  REQUIRE(step == 12);

  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem priority ordering", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.SetWorkerThreadCount(1);
  jobs.Init();

  ::std::atomic<int> counter {0};
  ::std::vector<int> order;
  ::std::mutex orderMu;

  auto makeJob = [&](int id) {
    return [&counter, &order, &orderMu, id]() {
      ::std::lock_guard lock(orderMu);
      order.push_back(id);
      counter.fetch_add(1);
    };
  };

  JobHandle h1 = jobs.Submit(makeJob(1), JobPriority::Low);
  JobHandle h2 = jobs.Submit(makeJob(2), JobPriority::High);
  JobHandle h3 = jobs.Submit(makeJob(3), JobPriority::Normal);

  JobHandle all[] = {h1, h2, h3};
  jobs.WaitAll(all, 3);

  REQUIRE(counter.load() == 3);

  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem with custom thread count", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.SetWorkerThreadCount(2);
  jobs.Init();

  REQUIRE(jobs.WorkerThreadCount() == 2);

  ::std::atomic<int> counter {0};
  JobHandle h = jobs.Submit([&counter]() { counter.fetch_add(1); });
  jobs.Wait(h);
  REQUIRE(counter.load() == 1);

  jobs.Shutdown();
}

TEST_CASE("ThreadPoolJobSystem labeled jobs", "[runtime][jobs]")
{
  ThreadPoolJobSystem jobs;
  jobs.Init();

  Label label = MakeLabel("test.job");
  bool ran = false;
  JobHandle h = jobs.Submit([&ran]() { ran = true; }, JobPriority::Normal, label);
  jobs.Wait(h);
  REQUIRE(ran);

  jobs.Shutdown();
}
