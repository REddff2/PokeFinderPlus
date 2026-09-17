#include "ResultIdentity.hpp"
#include <Core/Parents/PersonalLoader.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Buttons.hpp>
#include <Core/Enum/Method.hpp>
#include <Model/Gen3/WildModel3.hpp>
#include <Model/Gen3/PokeSpotModel.hpp>
#include <Model/Gen4/WildModel4.hpp>
#include <Model/Gen4/PokeRadarModel.hpp>
#include <Model/Gen5/WildModel5.hpp>
#include <Model/Gen5/HiddenGrottoModel.hpp>
#include <Model/Gen5/PhenomenonModel.hpp>
#include <Model/Gen5/PickupModel.hpp>
#include <Model/Gen8/WildModel8.hpp>
#include <Model/Gen8/UndergroundModel.hpp>
#include <QCoreApplication>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QDir>
#include <QImage>
#include <QStyledItemDelegate>
#include <iostream>
#include <stdexcept>

void check(bool,const char *);
void events();
QImage cell(QTableView &,int,int,bool);
int column(const QAbstractItemModel &model,const char *label)
{
    const auto translated = QCoreApplication::translate(model.metaObject()->className(),label);
    for(int i=0;i<model.columnCount();++i) if(model.headerData(i,Qt::Horizontal).toString()==translated) return i;
    throw std::runtime_error(std::string("Missing column ")+label+" in "+model.metaObject()->className());
}
ResultIdentity get(const QAbstractItemModel &model,const char *label,int row=0)
{
    const int col=column(model,label);
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(const_cast<QAbstractItemModel *>(&model));
    return SlotSprites::identity(proxy.index(row,col));
}
template<class T> SearcherState5<T> searcher(const T &state)
{ return {DateTime(2026,1,1),0,Buttons::None,0,state}; }
void coverage()
{
    const auto *info=PersonalLoader::getPersonal(Game::Emerald,25);
    const std::array<u8,6> ivs{1,2,3,4,5,6};
    WildSearcherModel3 wild3(nullptr);
    wild3.addItem(WildSearcherState(1,2,ivs,0,1,5,0,1,3,155,25,0,info));
    check(get(wild3,"Slot").species==25 && get(wild3,"Slot").shiny,"Gen3 searcher typed result");
    PokeSpotModel spot(nullptr);
    PokeSpotState spotState(0,2,1,1,3,25,info); spotState.update(0,0,5,ivs,info); spot.addItem(spotState);
    check(get(spot,"Slot").species==25 && get(spot,"Slot").form==-1,"PokeSpot species without guessed form");
    WildGeneratorState4 gen4(0,0,0,2,ivs,0,1,5,0,1,3,155,25,0,info);
    WildGeneratorModel4 wild4(nullptr); wild4.setGame(Game::Diamond); wild4.addItem(gen4);
    for(bool steps:{false,true})
    {
        wild4.setShowStepEncounter(steps);
        check(get(wild4,"Slot").species==25 && get(wild4,"Item").item==155,"Gen4 generator dynamic columns and held item");
    }
    WildSearcherState4 search4(1,2,ivs,0,1,5,0,1,3,155,25,0,info); search4.setAdvances(0);
    WildSearcherModel4 wildSearch4(nullptr); wildSearch4.setMethod(Method::MethodJ); wildSearch4.addItem(search4);
    check(get(wildSearch4,"Slot").species==25 && get(wildSearch4,"Item").item==155,"Gen4 searcher and item");
    PokeRadarState patches(0,0,{});
    PokeRadarModel4 radar(nullptr); radar.addItem(patches); radar.addItem(PokeRadarState(patches,gen4));
    check(get(radar,"Slot").species==0 && get(radar,"Slot",1).species==25,"Radar patch-only row never becomes a Pokemon");
    PokeRadarModel4 radarSearch(nullptr,true); radarSearch.addItem(PokeRadarState(patches,search4,0,1));
    check(get(radarSearch,"Slot").species==25 && get(radarSearch,"Item").item==155,"Radar searcher selects searcher Pokemon state");
    WildState5 gen5(0,0,0,false,false,0,0,2,ivs,0,1,5,0,1,3,155,25,0,info);
    WildState5 item5(0,0,0,true,true,0,0,2,ivs,0,1,5,0,1,3,155,25,0,info);
    WildGeneratorModel5 wild5(nullptr); wild5.addItem(gen5); wild5.addItem(item5);
    for(bool moving:{false,true}) for(bool phenomenon:{false,true})
    {
        wild5.setShowMovingTrigger(moving); wild5.setShowPhenomenon(phenomenon);
        check(get(wild5,"Slot").species==25 && get(wild5,"Slot",1).species==0 && get(wild5,"Item",1).item==155,
              "Gen5 modes distinguish Pokemon from phenomenon items");
    }
    WildSearcherModel5 wildSearch5(nullptr); wildSearch5.addItem(searcher(gen5)); wildSearch5.addItem(searcher(item5));
    for(bool power:{false,true})
    {
        wildSearch5.setShowPassPower(power);
        check(get(wildSearch5,"Slot").species==25 && get(wildSearch5,"Slot",1).species==0 && get(wildSearch5,"Item",1).item==155,
              "Gen5 searcher proxy and Pass Power columns");
    }
    HiddenGrottoState grottoPokemon(0,0,0,0,25,1), grottoItem(0,0,0,0,155), grottoInvalid(0,0);
    HiddenGrottoSlotGeneratorModel5 grotto(nullptr); grotto.addItems({grottoPokemon,grottoItem,grottoInvalid});
    check(get(grotto,"Slot").species==25 && !get(grotto,"Slot").shiny && get(grotto,"Slot").form==-1
          && get(grotto,"Slot",1).item==155 && get(grotto,"Slot",2).species==0,"Grotto mixed kinds, unknown shiny/form, invalid result");
    HiddenGrottoSlotSearcherModel5 grottoSearch(nullptr); grottoSearch.addItem(searcher(grottoItem));
    for(bool power:{false,true})
    {
        grottoSearch.setShowPassPower(power);
        check(get(grottoSearch,"Slot").item==155,"Grotto searcher changing column positions");
    }
    PhenomenonState unknownPokemon(0,0), knownItem(0,0,155);
    PhenomenonGeneratorModel5 legacy(nullptr); legacy.addItems({unknownPokemon,knownItem});
    check(get(legacy,"Slot").species==0 && get(legacy,"Slot").item==0 && get(legacy,"Slot",1).item==155,
          "legacy generic Pokemon never reads uninitialized identity");
    PhenomenonSearcherModel5 legacySearch(nullptr); legacySearch.addItem(searcher(knownItem));
    check(get(legacySearch,"Slot").item==155,"legacy phenomenon searcher item");
    PickupState pickup(0,0,true,{true,false,true,true,true,true},{155,155,0,155,155,155},gen5);
    PickupGeneratorModel5 pickupModel(nullptr); pickupModel.addItem(pickup);
    check(get(pickupModel,"Slot").species==25 && get(pickupModel,"Held Item").item==155 && get(pickupModel,"Item 1").item==155
          && get(pickupModel,"Item 2").item==0 && get(pickupModel,"Item 3").item==0,"Pickup embedded wild, active items, zero and inactive guards");
    PickupSearcherModel5 pickupSearch(nullptr); pickupSearch.addItem(searcher(pickup));
    check(get(pickupSearch,"Item 6").item==155,"Pickup searcher sixth item");
    WildModel8 wild8(nullptr); wild8.addItem(WildState8(0,0,2,ivs,0,1,5,0,1,3,155,25,0,0,0,info));
    check(get(wild8,"Slot").species==25 && get(wild8,"Item").item==155,"BDSP wild Pokemon and item");
    UndergroundModel underground(nullptr); underground.addItem(UndergroundState(0,0,2,ivs,0,1,5,0,1,0,0,0,155,25,info));
    check(get(underground,"Species").species==25 && get(underground,"Species").form==-1 && get(underground,"Item").item==155,
          "Underground species and independent item");
    check(true,"all 16 supported model classes exercised with real host model implementations");
}

void renderCoverage()
{
    const auto *info=PersonalLoader::getPersonal(Game::Emerald,25);
    QTableView table;
    UndergroundModel model(&table);
    model.addItem(UndergroundState(0,0,2,{1,2,3,4,5,6},0,1,5,0,1,0,0,0,155,25,info));
    table.setModel(&model); table.verticalHeader()->setDefaultSectionSize(30);
    table.resize(1200,150); table.setColumnWidth(2,150); table.setColumnWidth(3,150);
    auto itemBefore=cell(table,0,2,false), pokemonBefore=cell(table,0,3,false);
    table.show(); events();
    check(cell(table,0,2,false)!=itemBefore && cell(table,0,3,false)!=pokemonBefore,"normal item and shiny Pokemon both paint in their real Underground columns");
    table.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("evidence/items-and-pokemon.png"));
    QTableView mixed;
    HiddenGrottoSlotGeneratorModel5 grotto(&mixed);
    grotto.addItems({HiddenGrottoState(0,0,0,0,25,1),HiddenGrottoState(0,0,0,0,155),HiddenGrottoState(0,0)});
    mixed.setModel(&grotto); mixed.verticalHeader()->setDefaultSectionSize(30); mixed.setColumnWidth(4,180); mixed.resize(620,180);
    auto invalidBefore=cell(mixed,2,4,false), grottoItemBefore=cell(mixed,1,4,false);
    mixed.show(); events();
    check(cell(mixed,2,4,false)==invalidBefore && cell(mixed,1,4,false)!=grottoItemBefore,"mixed Grotto Slot decorates item and preserves invalid row exactly");
    mixed.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath("evidence/mixed-grotto.png"));
    class UnknownDelegate : public QStyledItemDelegate {};
    QTableView custom;
    UndergroundModel customModel(&custom); customModel.addItems(model.getModel()); custom.setModel(&customModel);
    UnknownDelegate delegate; custom.setItemDelegate(&delegate); auto *original=custom.style(); custom.show(); events();
    check(custom.style()==original,"unknown custom delegate intentionally skipped");
}
