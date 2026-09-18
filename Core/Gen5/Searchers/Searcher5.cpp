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

#include "Searcher5.hpp"
#include <Core/RNG/SHA1.hpp>
#include <Core/Enum/Buttons.hpp>
#include <algorithm>
#include <Core/Util/DateTime.hpp>

template <class Generator, class State>
Searcher5<Generator, State>::Searcher5(const Generator &generator, const Profile5 &profile, std::shared_ptr<GpuWild::Session> gpuSession) :
    SearcherBase5<Generator, State>(generator, profile)
{
    if constexpr (requires { generator.gpuIVPlan(); })
    {
        gpuPlan = generator.gpuIVPlan();
        if (SearchOptimization::gpuEnabled(Gen5::optimizationFamily<Generator>) && gpuPlan)
        {
            try { gpu = gpuSession ? std::move(gpuSession) : std::make_shared<GpuWild::Session>(); }
            catch (const std::exception &) { /* Optional GPU: use Smart CPU. */ }
        }
    }
}

template <class Generator, class State>
void Searcher5<Generator, State>::search(const Date &start, const Date &end)
{
    if (gpu && (!gpuPlan || this->candidatesPerWorker >= 8ULL * GpuWild::Session::BatchSize) && searchGpu(start, end)) return;
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
                        auto states = this->generator.generate(seeds[i]);
                        if (!states.empty())
                        {
                            DateTime dt(day, time + i);

                            std::lock_guard<std::mutex> lock(this->mutex);
                            if (!smartResults) this->results.reserve(this->results.capacity() + states.size());
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
bool Searcher5<Generator, State>::searchGpu(const Date &start, const Date &end)
{
    if constexpr (!requires { this->generator.gpuIVPlan(); }) return false;
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
            auto indices = gpu->filter(batch, this->cancelled, { this->generator.getMinIVs(), this->generator.getMaxIVs() }, gpuPlan, Gen5::optimizationFamily<Generator>);
            auto finish = [&](u32 index) {
                auto states = this->generator.generate(batch[index]);
                if (states.empty()) return;
                auto found = std::upper_bound(segments.begin(), segments.end(), index,
                    [](size_t value, const Segment &segment) { return value < segment.begin; });
                const auto &meta = *std::prev(found);
                DateTime dt(meta.day, meta.time + u32(index - meta.begin));
                std::lock_guard<std::mutex> lock(this->mutex);
                // Amortized vector growth preserves all occurrences.
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

#include <Core/Gen5/Generators/DreamRadarGenerator.hpp>
#include <Core/Gen5/Generators/EggGenerator5.hpp>
#include <Core/Gen5/Generators/EventGenerator5.hpp>
#include <Core/Gen5/Generators/HiddenGrottoGenerator.hpp>
#include <Core/Gen5/Generators/IDGenerator5.hpp>
#include <Core/Gen5/Generators/PickupGenerator.hpp>
#include <Core/Gen5/States/DreamRadarState.hpp>
#include <Core/Gen5/States/EggState5.hpp>
#include <Core/Gen5/States/EventState5.hpp>
#include <Core/Gen5/States/HiddenGrottoState.hpp>
#include <Core/Gen5/States/PickupState.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Gen5/States/State5.hpp>
#include <Core/Parents/States/IDState.hpp>

template class Searcher5<DreamRadarGenerator, DreamRadarState>;
template class Searcher5<EventGenerator5, EventState5>;
template class Searcher5<EggGenerator5, EggState5>;
template class Searcher5<HiddenGrottoSlotGenerator, HiddenGrottoState>;
template class Searcher5<IDGenerator5, IDState>;
template class Searcher5<PickupGenerator, PickupState>;
