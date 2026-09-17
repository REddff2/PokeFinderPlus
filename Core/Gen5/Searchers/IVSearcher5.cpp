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

#include "IVSearcher5.hpp"
#include <Core/Enum/Buttons.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/RNG/SHA1.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Util/SearchMetrics.hpp>
#include <type_traits>

template <class Generator, class Result>
static void reserveWildResults(std::vector<Result> &results, size_t count, const Generator &generator)
{
    if constexpr (std::is_same_v<Generator, WildGenerator5>)
    {
        // Let vector's amortized growth handle Wild batches. Reserving capacity
        // plus count forced a reallocation and full copy for every seed batch.
        if (generator.optimizedPruningEnabled()) return;
    }
    results.reserve(results.capacity() + count);
}


template <class Generator, class State>
IVSearcher5<Generator, State>::IVSearcher5(u32 initialAdvances, u32 maxAdvances, const Generator &generator, const Profile5 &profile,
                                             std::shared_ptr<GpuWild::Session> gpuSession) :
    SearcherBase5<Generator, State>(generator, profile), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
    if constexpr (std::is_same_v<Generator, WildGenerator5>)
    {
        if (SearchOptimization::gpuEnabled() && generator.payloadFirstEnabled(initialAdvances, maxAdvances))
        {
            try { gpu = gpuSession ? std::move(gpuSession) : std::make_shared<GpuWild::Session>(); }
            catch (const std::exception &) { /* Optional GPU allocation failure: use original CPU loop. */ }
        }
    }
}

template <class Generator, class State>
void IVSearcher5<Generator, State>::search(const Date &start, const Date &end)
{
    if (gpu && searchGpu(start, end)) return;
    SHA1SSE sha(this->profile);
    while (true)
    {
        Date day = start + this->index.fetch_add(1, std::memory_order_relaxed);
        if (day > end)
        {
            break;
        }

        sha.setDate(day);
        for (u16 timer0 = this->profile.getTimer0Min(); timer0 <= this->profile.getTimer0Max(); timer0++)
        {
            sha.setTimer0(timer0, this->profile.getVCount());
            auto alpha = sha.precompute();
            for (const auto &keypress : this->keypresses)
            {
                sha.setButton(keypress.value);
                for (u32 time = 0; time < 86400; time += 4)
                {
                    if (this->cancelled.load(std::memory_order_relaxed))
                    {
                        return;
                    }

                    sha.setTime(time, this->profile.getDSType());
                    auto seeds = sha.hashSeed(alpha);

                    for (u32 i = 0; i < seeds.size(); i++)
                    {
                        auto states = this->generator.generate(seeds[i], initialAdvances, maxAdvances);
                        if (!states.empty())
                        {
                            DateTime dt(day, time + i);

                            std::lock_guard<std::mutex> lock(this->mutex);
                            reserveWildResults(this->results, states.size(), this->generator);
                            for (const auto &state : states)
                            {
                                #ifdef POKEFINDER_SEARCH_INSTRUMENTATION
                                if constexpr (std::is_same_v<Generator, WildGenerator5>)
                                {
                                    if (!this->generator.optimizedPruningEnabled()
                                        ? &state == &states.front() : this->results.size() == this->results.capacity())
                                    { POKEFINDER_SEARCH_COUNT(wildResultReallocations); }
                                }
                                #endif
                                this->results.emplace_back(dt, seeds[i], keypress.button, timer0, state);
                            }
                        }
                    }
                }
                this->progress.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

// The guarded GPU path preserves original SHA seeds and every metadata occurrence.
// No batch is published until it completes; failure replays that whole batch on CPU.
template <class Generator, class State>
bool IVSearcher5<Generator, State>::searchGpu(const Date &start, const Date &end)
{
    if constexpr (!std::is_same_v<Generator, WildGenerator5>) return false;
    else
    {
        struct Segment { size_t begin; Date day; u32 time; Buttons buttons; u16 timer; };
        std::vector<u64> batch;
        std::vector<Segment> segments;
        try
        {
            batch.reserve(GpuWild::Session::BatchSize);
            // A batch spans at most 14 86,400-second key/day segments.
            segments.reserve(32);
        }
        catch (const std::exception &) { return false; } // No date acquired yet.

        auto consume = [&] {
            if (batch.empty() || this->cancelled.load(std::memory_order_relaxed)) return;
            auto indices = gpu->filter(batch, this->cancelled, { this->generator.getMinIVs(), this->generator.getMaxIVs() });
            auto finish = [&](u32 index) {
                auto states = this->generator.generate(batch[index], initialAdvances, maxAdvances);
                if (states.empty()) return;
                auto found = std::upper_bound(segments.begin(), segments.end(), index,
                    [](size_t value, const Segment &segment) { return value < segment.begin; });
                const auto &meta = *std::prev(found);
                DateTime dt(meta.day, meta.time + u32(index - meta.begin));
                std::lock_guard<std::mutex> lock(this->mutex);
                reserveWildResults(this->results, states.size(), this->generator);
                for (const auto &state : states)
                    this->results.emplace_back(dt, batch[index], meta.buttons, meta.timer, state);
            };
            if (indices)
            {
                for (u32 index : *indices)
                {
                    if (this->cancelled.load(std::memory_order_relaxed)) break;
                    finish(index);
                }
            }
            else
            {
                for (u32 index = 0; index < batch.size(); ++index)
                {
                    if (this->cancelled.load(std::memory_order_relaxed)) break;
                    finish(index);
                }
            }
            batch.clear(); segments.clear();
        };

        SHA1SSE sha(this->profile);
        while (true)
        {
            Date day = start + this->index.fetch_add(1, std::memory_order_relaxed);
            if (day > end) break;
            sha.setDate(day);
            for (u16 timer0 = this->profile.getTimer0Min(); timer0 <= this->profile.getTimer0Max(); ++timer0)
            {
                sha.setTimer0(timer0, this->profile.getVCount());
                auto alpha = sha.precompute();
                for (const auto &keypress : this->keypresses)
                {
                    sha.setButton(keypress.value);
                    for (u32 time = 0; time < 86400; time += 4)
                    {
                        if (this->cancelled.load(std::memory_order_relaxed)) return true;
                        if (time == 0 || batch.empty()) segments.push_back({ batch.size(), day, time, keypress.button, timer0 });
                        sha.setTime(time, this->profile.getDSType());
                        auto seeds = sha.hashSeed(alpha);
                        batch.insert(batch.end(), seeds.begin(), seeds.end());
                        if (batch.size() == GpuWild::Session::BatchSize) consume();
                    }
                    this->progress.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        consume();
        return true;
    }
}

template <class Generator, class State>
IVSearcher5Fast<Generator, State>::IVSearcher5Fast(u32 initialAdvances, u32 maxAdvances,
                                                   const fph::MetaFphMap<u64, std::array<u8, 6>> &ivCache, const Generator &generator,
                                                   const Profile5 &profile) :
    SearcherBase5<Generator, State>(generator, profile), ivCache(ivCache), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
}

template <class Generator, class State>
void IVSearcher5Fast<Generator, State>::search(const Date &start, const Date &end)
{
    SHA1SSE sha(this->profile);
    while (true)
    {
        Date day = start + this->index.fetch_add(1, std::memory_order_relaxed);
        if (day > end)
        {
            break;
        }

        sha.setDate(day);
        for (u16 timer0 = this->profile.getTimer0Min(); timer0 <= this->profile.getTimer0Max(); timer0++)
        {
            sha.setTimer0(timer0, this->profile.getVCount());
            auto alpha = sha.precompute();
            for (const auto &keypress : this->keypresses)
            {
                sha.setButton(keypress.value);
                for (u32 time = 0; time < 86400; time += 4)
                {
                    if (this->cancelled.load(std::memory_order_relaxed))
                    {
                        return;
                    }

                    sha.setTime(time, this->profile.getDSType());
                    auto seeds = sha.hashSeed(alpha);

                    for (u32 i = 0; i < seeds.size(); i++)
                    {
                        for (u64 j = initialAdvances; j <= (initialAdvances + maxAdvances); j++)
                        {
                            const auto entry = ivCache.find((j << 32) | (seeds[i] >> 32));
                            if (entry == ivCache.end())
                            {
                                continue;
                            }

                            auto states = this->generator.generate(seeds[i], { { j, entry->second } });
                            if (!states.empty())
                            {
                                DateTime dt(day, time + i);

                                std::lock_guard<std::mutex> lock(this->mutex);
                                reserveWildResults(this->results, states.size(), this->generator);
                                for (const auto &state : states)
                                {
                                    #ifdef POKEFINDER_SEARCH_INSTRUMENTATION
                                if constexpr (std::is_same_v<Generator, WildGenerator5>)
                                {
                                    if (!this->generator.optimizedPruningEnabled()
                                        ? &state == &states.front() : this->results.size() == this->results.capacity())
                                    { POKEFINDER_SEARCH_COUNT(wildResultReallocations); }
                                }
                                #endif
                                this->results.emplace_back(dt, seeds[i], keypress.button, timer0, state);
                                }
                            }
                        }
                    }
                }
                this->progress.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

template <class Generator, class State>
IVSearcher5CacheFast<Generator, State>::IVSearcher5CacheFast(u32 initialAdvances, u32 maxAdvances,
                                                             const fph::MetaFphMap<u64, u64> &sha1Cache,
                                                             const fph::MetaFphMap<u64, std::array<u8, 6>> &ivCache,
                                                             const Generator &generator, const Profile5 &profile) :
    SearcherBase5<Generator, State>(generator, profile),
    sha1Cache(sha1Cache),
    ivCache(ivCache),
    initialAdvances(initialAdvances),
    maxAdvances(maxAdvances)
{
}

template <class Generator, class State>
void IVSearcher5CacheFast<Generator, State>::search(const Date &start, const Date &end)
{
    SHA1Key key;
    while (true)
    {
        Date day = start + this->index.fetch_add(1, std::memory_order_relaxed);
        if (day > end)
        {
            break;
        }

        key.date = day.getJD() - Date().getJD();
        for (u16 timer0 = this->profile.getTimer0Min(); timer0 <= this->profile.getTimer0Max(); timer0++)
        {
            key.timer0 = timer0;
            for (const auto &keypress : this->keypresses)
            {
                key.button = toInt(keypress.button);
                for (u32 time = 0; time < 86400; time++)
                {
                    if (this->cancelled.load(std::memory_order_relaxed))
                    {
                        return;
                    }

                    key.time = time;

                    const auto sha1Entry = sha1Cache.find(key.key);
                    if (sha1Entry == sha1Cache.end())
                    {
                        continue;
                    }

                    u64 seed = sha1Entry->second;
                    for (u64 j = initialAdvances; j <= (initialAdvances + maxAdvances); j++)
                    {
                        const auto ivEntry = ivCache.find((j << 32) | (seed >> 32));
                        if (ivEntry == ivCache.end())
                        {
                            continue;
                        }

                        auto states = this->generator.generate(seed, { { j, ivEntry->second } });
                        if (!states.empty())
                        {
                            DateTime dt(day, time);

                            std::lock_guard<std::mutex> lock(this->mutex);
                            reserveWildResults(this->results, states.size(), this->generator);
                            for (const auto &state : states)
                            {
                                #ifdef POKEFINDER_SEARCH_INSTRUMENTATION
                                if constexpr (std::is_same_v<Generator, WildGenerator5>)
                                {
                                    if (!this->generator.optimizedPruningEnabled()
                                        ? &state == &states.front() : this->results.size() == this->results.capacity())
                                    { POKEFINDER_SEARCH_COUNT(wildResultReallocations); }
                                }
                                #endif
                                this->results.emplace_back(dt, seed, keypress.button, timer0, state);
                            }
                        }
                    }
                }
                this->progress.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

#include <Core/Gen5/Generators/HiddenGrottoGenerator.hpp>
#include <Core/Gen5/Generators/StaticGenerator5.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Gen5/States/State5.hpp>
#include <Core/Gen5/States/WildState5.hpp>

template class IVSearcher5<HiddenGrottoGenerator, State5>;
template class IVSearcher5Fast<HiddenGrottoGenerator, State5>;
template class IVSearcher5CacheFast<HiddenGrottoGenerator, State5>;

template class IVSearcher5<StaticGenerator5, State5>;
template class IVSearcher5Fast<StaticGenerator5, State5>;
template class IVSearcher5CacheFast<StaticGenerator5, State5>;

template class IVSearcher5<WildGenerator5, WildState5>;
template class IVSearcher5Fast<WildGenerator5, WildState5>;
template class IVSearcher5CacheFast<WildGenerator5, WildState5>;
