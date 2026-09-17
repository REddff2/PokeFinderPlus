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

#ifndef PHASE2_WILDGENERATOR5_HPP
#define PHASE2_WILDGENERATOR5_HPP

#include <Core/Enum/Lead.hpp>
#include <Core/Util/SearchOptimization.hpp>
#include <Core/Gen5/EncounterArea5.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/Filters/StateFilter.hpp>
#include <Core/Parents/Generators/WildGenerator.hpp>
#include <vector>

class WildState5;

#include <Core/Gen5/Generators/WildGenerator5.hpp>

class Phase2WildGenerator5 : public WildGenerator<EncounterArea5, Profile5, WildStateFilter>
{
public:
    /**
     * @brief Construct a new Phase2WildGenerator5 object
     *
     * @param initialAdvances Initial number of advances
     * @param maxAdvances Maximum number of advances
     * @param offset Number of advances to offset
     * @param method Encounter method
     * @param lead Encounter lead
     * @param passPower Pass power
     * @param searchMovingTrigger Calculate moving battle trigger ratio
     * @param requireMovingTrigger Only return states with a possible moving battle trigger
     * @param area Wild pokemon info
     * @param profile Profile Information
     * @param filter State filter
     */
    Phase2WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, u8 passPower, bool searchMovingTrigger,
                   bool requireMovingTrigger, const EncounterArea5 &area, const Profile5 &profile, const WildStateFilter &filter);

    Phase2WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, const std::vector<u8> &passPowers,
                   bool searchMovingTrigger, bool requireMovingTrigger, const EncounterArea5 &area, const Profile5 &profile,
                   const WildStateFilter &filter, bool requirePassPowerIVAdvance = false);

    /**
     * @brief Construct a new Phase2WildGenerator5 object
     *
     * @param initialAdvances Initial number of advances
     * @param maxAdvances Maximum number of advances
     * @param offset Number of advances to offset
     * @param method Encounter method
     * @param leads Encounter leads
     * @param luckyPower Lucky power level
     * @param area Wild pokemon info
     * @param profile Profile Information
     * @param filter State filter
     */
    Phase2WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, const std::vector<Lead> &leads, u8 luckyPower,
                   const EncounterArea5 &area, const Profile5 &profile, const WildStateFilter &filter);

    Phase2WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, const std::vector<Lead> &leads,
                   const std::vector<u8> &passPowers, bool searchMovingTrigger, bool requireMovingTrigger, const EncounterArea5 &area,
                   const Profile5 &profile, const WildStateFilter &filter, bool requirePassPowerIVAdvance = false,
                   bool filterNonRequiredLeads = true, bool optimizedPruning = SearchOptimization::pruningEnabled());

    /**
     * @brief Generates states for the \p encounterArea
     *
     * @param seed Starting PRNG state
     * @param initialAdvances Initial number of IV advances
     * @param maxAdvances Maximum number of IV advances
     *
     * @return Vector of computed states
     */
    std::vector<WildState5> generate(u64 seed, u32 initialAdvances, u32 maxAdvances) const;

    /**
     * @brief Generates states for the \p encounterArea
     *
     * @param seed Starting PRNG state
     * @param iv Vector of IV advances and IVs
     *
     * @return Vector of computed states
     */
    std::vector<WildState5> generate(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs) const;

    // Snapshot used by the Wild-only result accumulation policy.
    bool optimizedPruningEnabled() const { return optimizedPruning; }

private:
    bool optimizedPruning;
    bool pruneIVs;
    bool pruneHiddenPower;
    bool pruneSlots;
    bool prunePID;
    bool pruneLevel;
    std::vector<WildState5> generateFiltered(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs,
                                            bool ivsAlreadyFiltered) const;
    std::vector<u8> passPowers;
    std::vector<Lead> leads;
    bool searchMovingTrigger;
    bool requireMovingTrigger;
    bool requirePassPowerIVAdvance;
    bool filterNonRequiredLeads;

    std::vector<WildState5> generate(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs, u8 passPower, Lead lead) const;
};

#endif // PHASE2_WILDGENERATOR5_HPP
