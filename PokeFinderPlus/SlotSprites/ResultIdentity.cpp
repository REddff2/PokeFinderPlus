#include "ResultIdentity.hpp"
#include <Core/Parents/States/WildState.hpp>
#include <Core/Gen3/States/PokeSpotState.hpp>
#include <Core/Gen4/States/WildState4.hpp>
#include <Core/Gen4/States/PokeRadarState.hpp>
#include <Core/Gen5/States/WildState5.hpp>
#include <Core/Gen5/States/HiddenGrottoState.hpp>
#include <Core/Gen5/States/PhenomenonState.hpp>
#include <Core/Gen5/States/PickupState.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Gen8/States/WildState8.hpp>
#include <Core/Gen8/States/UndergroundState.hpp>
#include <Model/TableModel.hpp>
#include <QAbstractProxyModel>
#include <QCoreApplication>
#include <QSet>

namespace SlotSprites
{
const QAbstractItemModel *rootModel(const QAbstractItemModel *model)
{
    for (int depth = 0; model && depth < 16; ++depth)
    {
        const auto *proxy = qobject_cast<const QAbstractProxyModel *>(model);
        if (!proxy) return model;
        model = proxy->sourceModel();
    }
    return nullptr;
}
QModelIndex sourceIndex(QModelIndex index)
{
    for (int depth = 0; index.isValid() && depth < 16; ++depth)
    {
        const auto *proxy = qobject_cast<const QAbstractProxyModel *>(index.model());
        if (!proxy) return index;
        index = proxy->mapToSource(index);
    }
    return {};
}
bool supported(const QAbstractItemModel *model)
{
    const auto *root = rootModel(model);
    static const QSet<QByteArray> names = {
        "WildGeneratorModel3", "WildSearcherModel3", "PokeSpotModel",
        "WildGeneratorModel4", "WildSearcherModel4", "PokeRadarModel4",
        "WildGeneratorModel5", "WildSearcherModel5",
        "HiddenGrottoSlotGeneratorModel5", "HiddenGrottoSlotSearcherModel5",
        "PhenomenonGeneratorModel5", "PhenomenonSearcherModel5",
        "PickupGeneratorModel5", "PickupSearcherModel5", "WildModel8", "UndergroundModel"
    };
    return root && names.contains(root->metaObject()->className());
}
namespace
{
enum class Column { None, Pokemon, Item, Mixed, Pickup };
Column columnKind(const QAbstractItemModel *model, int column)
{
    const auto *root = rootModel(model);
    if (!root || !supported(root)) return Column::None;
    const QByteArray name(root->metaObject()->className());
    const QString header = model->headerData(column, Qt::Horizontal).toString();
    const auto is = [&](const char *text) { return header == QCoreApplication::translate(name.constData(), text); };
    if (is("Slot"))
        return name.startsWith("HiddenGrottoSlot") || name.startsWith("Phenomenon") ? Column::Mixed : Column::Pokemon;
    if (name == "UndergroundModel" && is("Species")) return Column::Pokemon;
    if (is("Item") || is("Held Item")) return Column::Item;
    if (name.startsWith("Pickup"))
        for (const char *label : {"Item 1","Item 2","Item 3","Item 4","Item 5","Item 6"})
            if (is(label)) return Column::Pickup;
    return Column::None;
}
template<class State> const State &row(const QModelIndex &index)
{
    // Only called after an exact concrete model name match. TableModel is the
    // first base in every audited model. No model is constructed or mutated.
    return static_cast<const TableModel<State> *>(index.model())->getItem(index.row());
}
template<class State> ResultIdentity pokemon(const State &state, Column kind)
{
    if constexpr (requires { state.isValid(); }) if (!state.isValid()) return {};
    if (kind == Column::Item)
    {
        if constexpr (requires { state.getItem(); }) return {0,-1,-1,false,state.getItem()};
        else return {};
    }
    if (kind != Column::Pokemon) return {};
    if constexpr (requires { state.getPhenomenonItem(); }) if (state.getPhenomenonItem()) return {};
    int form = -1;
    if constexpr (requires { state.getForm(); }) form = state.getForm();
    return {state.getSpecie(),form,state.getGender(),state.getShiny()!=0,0};
}
ResultIdentity grotto(const HiddenGrottoState &state)
{
    if (!state.isValid()) return {};
    if (state.getItem()) return {0,-1,-1,false,state.getData()};
    return {state.getData(),-1,state.getGender(),false,0};
}
ResultIdentity phenomenon(const PhenomenonState &state)
{
    // Non-item data is not initialized by the host constructor. Do not read it.
    return state.getItem() ? ResultIdentity{0,-1,-1,false,state.getData()} : ResultIdentity{};
}
ResultIdentity pickup(const PickupState &state, Column kind, const QModelIndex &index)
{
    if (kind == Column::Pickup)
    {
        const int slot = index.column() - 2; // Both audited Pickup root models: item columns 2..7.
        if (slot < 0 || slot > 5 || !state.getActive(static_cast<u8>(slot))) return {};
        return {0,-1,-1,false,state.getItem(static_cast<u8>(slot))};
    }
    return state.getWild() ? pokemon(*state.getWild(),kind) : ResultIdentity{};
}
}
bool spriteColumn(const QAbstractItemModel *model, int column)
{
    return columnKind(model,column) != Column::None;
}
ResultIdentity identity(const QModelIndex &viewIndex)
{
    const auto index = sourceIndex(viewIndex);
    if (!index.isValid() || index.parent().isValid() || index.row() >= index.model()->rowCount()) return {};
    const auto kind = columnKind(index.model(),index.column());
    if (kind == Column::None) return {};
    const QByteArray name(index.model()->metaObject()->className());
    if (name == "WildGeneratorModel3") return pokemon(row<WildGeneratorState>(index),kind);
    if (name == "WildSearcherModel3") return pokemon(row<WildSearcherState>(index),kind);
    if (name == "PokeSpotModel") return pokemon(row<PokeSpotState>(index),kind);
    if (name == "WildGeneratorModel4") return pokemon(row<WildGeneratorState4>(index),kind);
    if (name == "WildSearcherModel4") return pokemon(row<WildSearcherState4>(index),kind);
    if (name == "WildGeneratorModel5") return pokemon(row<WildState5>(index),kind);
    if (name == "WildSearcherModel5") return pokemon(row<SearcherState5<WildState5>>(index).getState(),kind);
    if (name == "WildModel8") return pokemon(row<WildState8>(index),kind);
    if (name == "UndergroundModel") return pokemon(row<UndergroundState>(index),kind);
    if (name == "HiddenGrottoSlotGeneratorModel5") return grotto(row<HiddenGrottoState>(index));
    if (name == "HiddenGrottoSlotSearcherModel5") return grotto(row<SearcherState5<HiddenGrottoState>>(index).getState());
    if (name == "PhenomenonGeneratorModel5") return phenomenon(row<PhenomenonState>(index));
    if (name == "PhenomenonSearcherModel5") return phenomenon(row<SearcherState5<PhenomenonState>>(index).getState());
    if (name == "PickupGeneratorModel5") return pickup(row<PickupState>(index),kind,index);
    if (name == "PickupSearcherModel5") return pickup(row<SearcherState5<PickupState>>(index).getState(),kind,index);
    if (name == "PokeRadarModel4")
    {
        const auto &state = row<PokeRadarState>(index);
        if (!state.hasPokemon()) return {};
        const int columns = index.model()->columnCount();
        if (columns == 27 && state.hasSearcherPokemon()) return pokemon(state.getSearcherPokemon(),kind);
        if ((columns == 27 || columns == 30) && !state.hasSearcherPokemon()) return pokemon(state.getPokemon(),kind);
    }
    return {};
}
}
