/*
 * Copyright 2014-2017 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define DISABLE_BOUNDS_CHECKS 1

#include <string>
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

#include "concurrent/Atomic64.h"
#include "SpscConcurrentArrayQueue.h"

static void BM_SpscQueueLatency(benchmark::State &state)
{
    int* i = new int{42};
    SpscConcurrentArrayQueue<int> qIn{1024};
    SpscConcurrentArrayQueue<int> qOut{1024};
    std::atomic<bool> start{false};
    std::atomic<bool> running{true};

    std::thread t(
        [&]()
        {
            start.store(true);

            while (running)
            {
                int* p = qIn.poll();
                if (p != nullptr)
                {
                    qOut.offer(p);
                }
                else
                {
                    //aeron::concurrent::atomic::cpu_pause();
                }
            }
        });

    while (!start)
    {
        ; // Spin
    }

    while (state.KeepRunning())
    {
        qIn.offer(i);
        while (qOut.poll() == nullptr)
        {
            ; // spin
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * sizeof(int));

    running.store(false);

    t.join();
}

BENCHMARK(BM_SpscQueueLatency)->UseRealTime();

static void BM_SpscQueueThroughput(benchmark::State &state)
{
    int* i = new int{42};
    SpscConcurrentArrayQueue<int> q{static_cast<int>(state.range(0))};
    std::atomic<bool> start{false};
    std::atomic<bool> running{true};
    std::int64_t totalMsgs = 0;

    std::thread t(
        [&]()
        {
            start.store(true);

            while (running)
            {
                if (nullptr == q.poll())
                {
                    aeron::concurrent::atomic::cpu_pause();
                }
            }
        });

    while (!start)
    {
        ; // Spin
    }

    while (state.KeepRunning())
    {
        while (!q.offer(i))
        {
            aeron::concurrent::atomic::cpu_pause();
        }
        totalMsgs++;
    }

    state.SetItemsProcessed(totalMsgs);
    state.SetBytesProcessed(totalMsgs * sizeof(int));

    running.store(false);

    t.join();
}

BENCHMARK(BM_SpscQueueThroughput)
    ->Arg(1024)
    ->Arg(64 * 1024)
    ->Arg(128 * 1024)
    ->Arg(256 * 1024)
    ->UseRealTime();

BENCHMARK_MAIN();