// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See commandqueue.h (the ownership invariant lives there). The drain-side
// executor for every tag lives in apiscript.cpp (ScriptCommand_Execute) -
// the game-touching bodies.

#include "commandqueue.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

namespace xuoassist {

namespace {

constexpr uint32_t DEFAULT_CAPACITY = 512;
constexpr uint32_t PRINT_THROTTLE_MS = 100;

// One queued call. The queue owns the node from push until the reclaim pass
// deletes it; the caller's XA_SCRIPT_COMMAND record is only linked through
// Owner for the result copyback (skipped when the waiter abandoned).
struct QUEUE_NODE
{
    XA_SCRIPT_COMMAND Command; // input snapshot at push + result side
    XA_SCRIPT_COMMAND *Owner{ nullptr };

    std::mutex Mtx;
    std::condition_variable Cv;
    bool Done{ false };      // completion path ran (results/copyback settled)
    bool Abandoned{ false }; // the waiter timed out; never copy back
    bool Ack{ false };       // the waiter read the results; safe to reclaim
};

std::mutex s_QueueMutex;
std::deque<QUEUE_NODE *> s_Queue;     // pending, drain-executes in order
std::deque<QUEUE_NODE *> s_Retired;   // completed, waiting Ack/Abandoned
uint32_t s_Capacity = DEFAULT_CAPACITY;
bool s_Shutdown = false;

std::mutex s_ThrottleMutex;
uint32_t s_LastPrintTick = 0;
uint32_t s_Suppressed = 0;

// Completes a node the way every path does: copy the results into the
// caller's record (unless the waiter abandoned) and release the waiter. The
// caller parks the node on s_Retired afterwards (it knows its own locking).
void complete_node(QUEUE_NODE *node)
{
    {
        std::lock_guard<std::mutex> lock(node->Mtx);
        if (!node->Abandoned && node->Owner != nullptr)
        {
            XA_SCRIPT_COMMAND &owner = *node->Owner;
            owner.ResultInt = node->Command.ResultInt;
            owner.ResultSerial = node->Command.ResultSerial;
            owner.ResultBool = node->Command.ResultBool;
            owner.ResultDropped = node->Command.ResultDropped;
            owner.ResultText = node->Command.ResultText;
            owner.ResultList = node->Command.ResultList;
            owner.PointerA = node->Command.PointerA;
            // the position getters report Y through IntA (the request slot);
            // no other executor mutates IntA, so copying it back carries the
            // result and leaves the request value intact for every other tag
            owner.IntA = node->Command.IntA;
        }
        else if (node->Owner != nullptr)
        {
            // abandoned in flight: the reader must see "no data", not a real
            // zero left over from the executor's run
            node->Owner->ResultDropped = true;
        }
        node->Done = true;
    }
    node->Cv.notify_one();
}

// Drop-oldest policy: the oldest entry is completed with Dropped so its
// script thread wakes immediately and sees "no data".
void drop_oldest_locked()
{
    QUEUE_NODE *victim = s_Queue.front();
    s_Queue.pop_front();
    victim->Command.ResultDropped = true;
    complete_node(victim);
    s_Retired.push_back(victim);
}

// Deletes retired nodes whose waiter acknowledged the results or abandoned
// the wait. Any thread may run it; the node mutex makes the flags visible.
void reclaim_locked()
{
    for (auto it = s_Retired.begin(); it != s_Retired.end();)
    {
        QUEUE_NODE *node = *it;
        bool reclaimable = false;
        {
            std::lock_guard<std::mutex> lock(node->Mtx);
            reclaimable = node->Done && (node->Ack || node->Abandoned);
        }
        if (reclaimable)
        {
            it = s_Retired.erase(it);
            delete node;
        }
        else
        {
            ++it;
        }
    }
}

} // namespace

void ScriptQueue_Exec(XA_SCRIPT_COMMAND *command, uint32_t timeoutMs)
{
    if (command == nullptr)
    {
        return;
    }
    command->ResultDropped = false;
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        reclaim_locked();
    }

    auto *node = new QUEUE_NODE();
    node->Owner = command;
    node->Command = *command; // input snapshot; the caller may not outlive it

    bool queued = false;
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        if (!s_Shutdown)
        {
            while (s_Queue.size() >= s_Capacity)
            {
                drop_oldest_locked();
            }
            s_Queue.push_back(node);
            queued = true;
        }
    }

    if (!queued)
    {
        // shutdown raced the push: the node never entered the queue, so it is
        // still solely ours - delete it and fail fast with a dropped result
        delete node;
        command->ResultDropped = true;
        return;
    }

    bool completed = false;
    {
        std::unique_lock<std::mutex> lock(node->Mtx);
        // The timeout keeps a deadlocked main thread from hanging the script
        // forever; the script then sees Dropped and its Wait()/call returns
        // "no data".
        completed = node->Cv.wait_for(
            lock, std::chrono::milliseconds(timeoutMs), [&] { return node->Done; });
        if (completed)
        {
            node->Ack = true; // last node access; the drain may reclaim now
        }
        else
        {
            node->Abandoned = true; // last node access in the timeout path
        }
    }

    if (!completed)
    {
        // the drain owns the node now: either it is still pending (remove and
        // dispose it here - the drain cannot be inside it, only the drain
        // pops) or it is executing (leave it; the drain completes and reclaims
        // it). Either way the caller's record just reads as dropped; mark that
        // here so the reader never mistakes timeout for a real zero result.
        command->ResultDropped = true;
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        for (auto it = s_Queue.begin(); it != s_Queue.end(); ++it)
        {
            if (*it == node)
            {
                s_Queue.erase(it);
                delete node;
                break;
            }
        }
    }
}

void ScriptQueue_Drain()
{
    // Bounded per frame: a print loop can enqueue far faster than the frame
    // runs; the remainder wait for the next frame (the getters simply see a
    // slightly older world - the event-loop behavior).
    uint32_t budget = 64;
    for (;;)
    {
        QUEUE_NODE *node = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            if (s_Queue.empty() || budget == 0)
            {
                break;
            }
            node = s_Queue.front();
            s_Queue.pop_front();
            reclaim_locked();
        }

        // DRAIN-OWNED from here until the completion below.
        ScriptCommand_Execute(&node->Command); // apiscript.cpp (main thread)
        complete_node(node);
        {
            std::lock_guard<std::mutex> lock(s_QueueMutex);
            s_Retired.push_back(node);
            reclaim_locked();
        }
        budget--;
    }
    reclaim_locked();
}

void ScriptQueue_Shutdown()
{
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    s_Shutdown = true;
    while (!s_Queue.empty())
    {
        QUEUE_NODE *victim = s_Queue.front();
        s_Queue.pop_front();
        victim->Command.ResultDropped = true;
        complete_node(victim);
        s_Retired.push_back(victim);
    }
    reclaim_locked();
}

uint32_t ScriptQueue_Capacity()
{
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    return s_Capacity;
}

void ScriptQueue_Reset()
{
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    s_Shutdown = false;
}

void ScriptQueue_SetCapacity(uint32_t entries)
{
    std::lock_guard<std::mutex> lock(s_QueueMutex);
    s_Capacity = entries > 16 ? entries : 16;
}

bool ScriptQueue_PrintThrottled(uint32_t &suppressedCount)
{
    suppressedCount = 0;
    const uint32_t now = g_Ticks;
    std::lock_guard<std::mutex> lock(s_ThrottleMutex);
    if (s_LastPrintTick != 0 && (now - s_LastPrintTick) < PRINT_THROTTLE_MS)
    {
        s_Suppressed++;
        return false;
    }
    s_LastPrintTick = now;
    suppressedCount = s_Suppressed;
    s_Suppressed = 0;
    return true;
}

} // namespace xuoassist
