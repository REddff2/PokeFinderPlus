#include "Milestone2WildGenerator5.hpp"
#include <Core/Gen5/States/WildState5.hpp>
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

#include "Milestone2WildSearcher5.hpp"
#include <Core/Enum/Buttons.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/RNG/SHA1.hpp>
#include "Milestone2WildGenerator5.hpp"
#include <Core/Util/SearchMetrics.hpp>
#include <type_traits>

template <class Generator, class Result>
static void reserveWildResults(std::vector<Result> &results, size_t count, const Generator &generator)
{
    if constexpr (std::is_same_v<Generator, Milestone2WildGenerator5>)
    {
        // Let vector's amortized growth handle Wild batches. Reserving capacity
        // plus count forced a reallocation and full copy for every seed batch.
        if (generator.optimizedPruningEnabled()) return;
    }
    results.reserve(results.capacity() + count);
}


template <class Generator, class State>
Milestone2WildSearcher5<Generator, State>::Milestone2WildSearcher5(u32 initialAdvances, u32 maxAdvances, const Generator &generator, const Profile5 &profile) :
    SearcherBase5<Generator, State>(generator, profile), initialAdvances(initialAdvances), maxAdvances(maxAdvances)
{
}

template <class Generator, class State>
void Milestone2WildSearcher5<Generator, State>::search(const Date &start, const Date &end)
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
                            reserveWildResults(this->results, states.size(), this->generator);
                            for (const auto &state : states)
                            {
                                #ifdef POKEFINDER_SEARCH_INSTRUMENTATION
                                if constexpr (std::is_same_v<Generator, Milestone2WildGenerator5>)
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


template class SearcherBase5<Milestone2WildGenerator5, WildState5>;
template class Milestone2WildSearcher5<Milestone2WildGenerator5, WildState5>;
