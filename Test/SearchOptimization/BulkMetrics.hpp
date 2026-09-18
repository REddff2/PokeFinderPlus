#pragma once
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
namespace BulkMetrics {
using Clock = std::chrono::steady_clock;
enum Kind { Slot, Level, PID, Construct, Stats, Accumulate, LockWait, LockHeld, Count };
inline std::atomic<unsigned long long> ns[Count]{}, calls[Count]{};
inline std::atomic<unsigned long long> grows{}, relocated{}, copies{}, nodes{}, duplicateProbes{}, duplicates{}, batches{};
struct Timer {
    Kind kind; Clock::time_point began=Clock::now(); bool running=true;
    explicit Timer(Kind kind):kind(kind){}
    void stop() { if(running) { ns[kind].fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-began).count(),std::memory_order_relaxed); calls[kind].fetch_add(1,std::memory_order_relaxed); running=false; } }
    ~Timer(){stop();}
};
template<class V> void append(const V &v) {
    copies.fetch_add(1,std::memory_order_relaxed);
    if(v.size()==v.capacity()) { grows.fetch_add(1,std::memory_order_relaxed); relocated.fetch_add(v.size(),std::memory_order_relaxed); }
}
inline void reset() { for(int i=0;i<Count;++i){ns[i]=0;calls[i]=0;} grows=0;relocated=0;copies=0;nodes=0;duplicateProbes=0;duplicates=0;batches=0; }
inline void print() {
    const char *names[]={"slot","level","pid","construct","stats_nested","accumulate","lock_wait","lock_held"};
    for(int i=0;i<Count;++i) std::cout<<' '<<names[i]<<"_ms="<<double(ns[i].load())/1000000<<' '<<names[i]<<"_calls="<<calls[i].load();
    std::cout<<" vector_growth="<<grows<<" relocated_rows="<<relocated<<" explicit_copies="<<copies
        <<" index_nodes="<<nodes<<" duplicate_probes="<<duplicateProbes<<" duplicates="<<duplicates<<" merge_batches="<<batches;
}
}
