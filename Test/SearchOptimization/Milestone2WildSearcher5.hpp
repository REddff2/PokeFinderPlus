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

#ifndef MILESTONE2WILDSEARCHER5_HPP
#define MILESTONE2WILDSEARCHER5_HPP

#include <Core/Gen5/Searchers/SearcherBase5.hpp>
#include <fph/meta_fph_table.h>

/**
 * @brief Parent searcher class for Static/Wild Gen 5 generators
 *
 * @tparam Generator Generator class to use
 * @tparam State State class to use
 */
template <class Generator, class State>
class Milestone2WildSearcher5 final : public SearcherBase5<Generator, State>
{
public:
    /**
     * @brief Construct a new Milestone2WildSearcher5 object
     *
     * @param initialAdvances Minimum IV advances
     * @param maxAdvances Maximum IV advances
     * @param generator State generator
     * @param profile Profile information
     */
    Milestone2WildSearcher5(u32 initialAdvances, u32 maxAdvances, const Generator &generator, const Profile5 &profile);

private:
    u32 initialAdvances;
    u32 maxAdvances;

    /**
     * @brief Searches between the \p start and \p end dates
     *
     * @param start Start date
     * @param end End date
     */
    void search(const Date &start, const Date &end) override;
};


#endif
