/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "IVCacheSearcher.hpp"
#include <Core/Gen5/Generators/IVRNG.hpp>
#include <Core/RNG/MT.hpp>
#include <Core/RNG/RNGList.hpp>
#include <algorithm>
#include <fstream>

static u8 gen(MT &rng)
{
    return rng.next() >> 27;
}

template <typename Type>
static void write(std::ofstream &file, Type val)
{
    file.write(reinterpret_cast<char *>(&val), sizeof(val));
}

IVCacheSearcher::IVCacheSearcher(u32 initialAdvances, u32 maxAdvances, std::shared_ptr<GpuWild::Session> gpuSession) :
    SearcherBase<std::vector<u32>>(), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
    entralink.resize(maxAdvances + 5);
    results.resize(maxAdvances + 3);
    roamer.resize(maxAdvances + 1);
    if (smart && SearchOptimization::gpuEnabled(SearchOptimization::Family::CacheBuilders) && maxAdvances < 8
        && GpuWild::IVPlan{initialAdvances, false, maxAdvances + 1}.valid())
    {
        try { gpu = gpuSession ? std::move(gpuSession) : std::make_shared<GpuWild::Session>(); }
        catch (const std::exception &) { /* Optional GPU: use Smart CPU. */ }
    }
}

void IVCacheSearcher::startSearch(int threads, u32 start, u32 end)
{
    if (start > end) return;
    threads = std::max(1, threads);
    gpuWorkload = gpu && (u64(end) - start + 1) / threads >= 8ULL * GpuWild::Session::BatchSize;
    activeThreads.store(threads);
    for (int i = 0; i < threads; i++)
    {
        threadContainer.emplace_back([this, start, end] {
            search(start, end);
            activeThreads.fetch_sub(1);
        });
    }
}

void IVCacheSearcher::writeResults(std::string_view file)
{
    std::ofstream stream(file.data(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (stream.is_open())
    {
        // Write magic identifier: CRC32 of "IVCache"
        write(stream, 0xd08cb7c0);

        // Write cache advances
        write(stream, initialAdvances);
        write(stream, maxAdvances);

        // Write seed sizes
        for (int i = 0; i < entralink.size(); i++)
        {
            std::ranges::sort(entralink[i]);
            write<u32>(stream, entralink[i].size());
        }

        for (int i = 0; i < results.size(); i++)
        {
            std::ranges::sort(results[i]);
            write<u32>(stream, results[i].size());
        }

        for (int i = 0; i < roamer.size(); i++)
        {
            std::ranges::sort(roamer[i]);
            write<u32>(stream, roamer[i].size());
        }

        // Write seeds
        for (int i = 0; i < entralink.size(); i++)
        {
            stream.write(reinterpret_cast<char *>(entralink[i].data()), entralink[i].size() * sizeof(u32));
        }

        for (int i = 0; i < results.size(); i++)
        {
            stream.write(reinterpret_cast<char *>(results[i].data()), results[i].size() * sizeof(u32));
        }

        for (int i = 0; i < roamer.size(); i++)
        {
            stream.write(reinterpret_cast<char *>(roamer[i].data()), roamer[i].size() * sizeof(u32));
        }
    }
}

void IVCacheSearcher::search(u32 start, u32 end)
{
    if (gpuWorkload && searchGpu(start, end)) return;
    while (true)
    {
        if (cancelled.load(std::memory_order_relaxed))
        {
            return;
        }

        u64 idx = index.fetch_add(1, std::memory_order_relaxed);
        if (idx > u64(end) - start) {
            break;
        }

        u32 seed = start + idx;

        searchSeed(seed);

        progress.fetch_add(1, std::memory_order_relaxed);
    }
}

void IVCacheSearcher::searchSeed(u32 seed)
{
    Gen5::IVRNG rngList(seed, initialAdvances, u64(maxAdvances) + 32, smart);
    for (u32 i = 0; i <= maxAdvances + 4; i++, rngList.advanceState())
    {
        // Entralink
        rngList.advance(22);
        u8 hp = rngList.next();
        u8 atk = rngList.next();
        u8 def = rngList.next();
        u8 spa = rngList.next();
        u8 spd = rngList.next();
        u8 spe = rngList.next();
        if (hp >= 30 && def >= 30 && spd >= 30 && (atk >= 30 || spa >= 30) && (spe <= 1 || spe >= 30))
        {
            std::lock_guard<std::mutex> lock(mutex);
            entralink[i].emplace_back(seed);
        }

        // Normal
        if (i <= maxAdvances + 2)
        {
            rngList.resetState();

            hp = rngList.next();
            atk = rngList.next();
            def = rngList.next();
            spa = rngList.next();
            spd = rngList.next();
            spe = rngList.next();

            if (hp >= 30 && def >= 30 && spd >= 30 && (atk >= 30 || spa >= 30) && (spe <= 1 || spe >= 30))
            {
                std::lock_guard<std::mutex> lock(mutex);
                results[i].emplace_back(seed);
            }
        }

        // Roamer
        if (i <= maxAdvances)
        {
            rngList.resetState();
            rngList.advance(1);

            hp = rngList.next();
            atk = rngList.next();
            def = rngList.next();
            spd = rngList.next();
            spe = rngList.next();
            spa = rngList.next();

            if (hp >= 30 && def >= 30 && spd >= 30 && (atk >= 30 || spa >= 30) && spe >= 30)
            {
                std::lock_guard<std::mutex> lock(mutex);
                roamer[i].emplace_back(seed);
            }
        }
    }

}


// The GPU rejects only seeds failing every existing cache predicate. Replaying
// all uncommitted seeds on failure preserves file contents, offsets and counts.
bool IVCacheSearcher::searchGpu(u32 start, u32 end)
{
    std::vector<u64> batch;
    try { batch.reserve(GpuWild::Session::BatchSize); }
    catch (const std::exception &) { return false; }
    const GpuWild::IVBounds bounds{{0,0,0,0,0,0},{31,31,31,31,31,31}};
    const GpuWild::IVPlan plan{initialAdvances, false, maxAdvances + 1};
    const u64 total = u64(end) - start + 1;
    while (!cancelled.load(std::memory_order_relaxed))
    {
        const u64 begin = index.fetch_add(GpuWild::Session::BatchSize, std::memory_order_relaxed);
        if (begin >= total) break;
        const u32 count = std::min<u64>(GpuWild::Session::BatchSize, total - begin);
        batch.clear();
        for (u32 i = 0; i < count; ++i) batch.push_back(u64(start + begin + i) << 32);
        auto accepted = gpu->filter(batch, cancelled, bounds, plan, SearchOptimization::Family::CacheBuilders);
        if (accepted)
        {
            for (u32 i : *accepted)
            {
                if (cancelled.load(std::memory_order_relaxed)) return true;
                searchSeed(start + begin + i);
            }
        }
        else
        {
            for (u32 i = 0; i < count; ++i)
            {
                if (cancelled.load(std::memory_order_relaxed)) return true;
                searchSeed(start + begin + i);
            }
        }
        progress.fetch_add(count, std::memory_order_relaxed);
    }
    return true;
}
