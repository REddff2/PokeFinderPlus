#include "BaselineWildGenerator5.hpp"
#include <Core/Gen5/States/WildState5.hpp>
#include <Core/Util/SearchMetrics.hpp>
/*
 * This file is part of PokÃ©Finder
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

#include <Core/Util/SearchOptimization.hpp>
#include <Core/Gen5/Searchers/SearcherBase5.hpp>
#include <Core/Util/DateTime.hpp>

template <class Generator, class State>
SearcherBase5<Generator, State>::SearcherBase5(const Generator &generator, const Profile5 &profile) :
    SearcherBase<SearcherState5<State>>(), generator(generator), profile(profile), keypresses(Keypresses::getKeypresses(profile))
{
}

template <class Generator, class State>
u64 SearcherBase5<Generator, State>::getMaxProgress(const Date &start, const Date &end) const
{
    return keypresses.size() * (start.daysTo(end) + 1) * (profile.getTimer0Max() - profile.getTimer0Min() + 1);
}

template <class Generator, class State>
void SearcherBase5<Generator, State>::startSearch(int threads, const Date &start, const Date &end)
{
    auto days = start.daysTo(end) + 1;
    if (days < threads) threads = days;

    this->activeThreads.store(threads);
    for (int i = 0; i < threads; i++)
    {
        this->threadContainer.emplace_back([this, start, end] {
            search(start, end);
            this->activeThreads.fetch_sub(1);
        });
    }
}

/*
 * This file is part of PokÃ©Finder
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

#include "BaselineWildSearcher5.hpp"
#include <Core/Enum/Buttons.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/RNG/SHA1.hpp>

template <class Generator, class State>
BaselineWildSearcher5<Generator, State>::BaselineWildSearcher5(u32 initialAdvances, u32 maxAdvances, const Generator &generator, const Profile5 &profile) :
    SearcherBase5<Generator, State>(generator, profile), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
}

template <class Generator, class State>
void BaselineWildSearcher5<Generator, State>::search(const Date &start, const Date &end)
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
                        auto states = this->generator.generate(seeds[i], initialAdvances, maxAdvances);
                        if (!states.empty())
                        {
                            DateTime dt(day, time + i);

                            std::lock_guard<std::mutex> lock(this->mutex);
                            POKEFINDER_SEARCH_COUNT(wildResultReallocations);
                            this->results.reserve(this->results.capacity() + states.size());
                            for (const auto &state : states)
                            {
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

template <class Generator, class State>
BaselineWildSearcher5Fast<Generator, State>::BaselineWildSearcher5Fast(u32 initialAdvances, u32 maxAdvances,
                                                   const fph::MetaFphMap<u64, std::array<u8, 6>> &ivCache, const Generator &generator,
                                                   const Profile5 &profile) :
    SearcherBase5<Generator, State>(generator, profile), ivCache(ivCache), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
}

template <class Generator, class State>
void BaselineWildSearcher5Fast<Generator, State>::search(const Date &start, const Date &end)
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
                                POKEFINDER_SEARCH_COUNT(wildResultReallocations);
                            this->results.reserve(this->results.capacity() + states.size());
                                for (const auto &state : states)
                                {
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
BaselineWildSearcher5CacheFast<Generator, State>::BaselineWildSearcher5CacheFast(u32 initialAdvances, u32 maxAdvances,
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
void BaselineWildSearcher5CacheFast<Generator, State>::search(const Date &start, const Date &end)
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
                            POKEFINDER_SEARCH_COUNT(wildResultReallocations);
                            this->results.reserve(this->results.capacity() + states.size());
                            for (const auto &state : states)
                            {
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


template class SearcherBase5<BaselineWildGenerator5, WildState5>;
template class BaselineWildSearcher5<BaselineWildGenerator5, WildState5>;
template class BaselineWildSearcher5Fast<BaselineWildGenerator5, WildState5>;
template class BaselineWildSearcher5CacheFast<BaselineWildGenerator5, WildState5>;
