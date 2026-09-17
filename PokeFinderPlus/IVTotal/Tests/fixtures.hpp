#include <Model/Gen3/EggModel3.hpp>
#include <Model/Gen3/GameCubeModel.hpp>
#include <Model/Gen3/PokeSpotModel.hpp>
#include <Model/Gen3/StaticModel3.hpp>
#include <Model/Gen3/WildModel3.hpp>
#include <Model/Gen4/EggModel4.hpp>
#include <Model/Gen4/EventModel4.hpp>
#include <Model/Gen4/PokeRadarModel.hpp>
#include <Model/Gen4/StaticModel4.hpp>
#include <Model/Gen4/WildModel4.hpp>
#include <Model/Gen5/AdjacentSeedsModel.hpp>
#include <Model/Gen5/DreamRadarModel.hpp>
#include <Model/Gen5/EggModel5.hpp>
#include <Model/Gen5/EventModel5.hpp>
#include <Model/Gen5/HiddenGrottoModel.hpp>
#include <Model/Gen5/PickupModel.hpp>
#include <Model/Gen5/StaticModel5.hpp>
#include <Model/Gen5/WildModel5.hpp>
#include <Model/Gen8/EggModel8.hpp>
#include <Model/Gen8/StaticModel8.hpp>
#include <Model/Gen8/UndergroundModel.hpp>
#include <Model/Gen8/WildModel8.hpp>
#include <Core/Parents/PersonalLoader.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Buttons.hpp>
#include <Core/Util/DateTime.hpp>
template<class T> T fixture(int n);
inline std::array<u8,6> fixtureIVs(int n) {
 const int totals[]={100,2,186,10};int left=totals[n%4];std::array<u8,6> v{};
 for(auto &x:v){x=std::min(left,31);left-=x;}return v;
}
template<> inline AdjacentSeedsState fixture<AdjacentSeedsState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=AdjacentSeedsState(u32(0x12340000+n), DateTime(2025,1,1,12,0,n), Buttons::None, 0x1234, u32(n), fixtureIVs(n), u32(40+n), n==2);return state;}
template<> inline DreamRadarState fixture<DreamRadarState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=DreamRadarState(u8(n), u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline EggState5 fixture<EggState5>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=EggState5(u16(800+n*82), u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 0, 0, std::array<u8,6>{1,0,0,0,0,0}, info);return state;}
template<> inline EggState8 fixture<EggState8>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=EggState8(u32(100+n*20), u32(2000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, std::array<u8,6>{1,0,0,0,0,0}, u32(0x12340000+n), info);return state;}
template<> inline EventState5 fixture<EventState5>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=EventState5(u16(800+n*82), u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline GeneratorState fixture<GeneratorState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=GeneratorState(u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline GeneratorState4 fixture<GeneratorState4>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=GeneratorState4(u16(800+n*82), u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline SearcherState fixture<SearcherState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=SearcherState(u32(0x12340000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline SearcherState4 fixture<SearcherState4>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=SearcherState4(u32(0x12340000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);state.setAdvances(100+n*20);return state;}
template<> inline State5 fixture<State5>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=State5(u16(800+n*82), u32(100+n*20), u32(n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, info);return state;}
template<> inline State8 fixture<State8>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=State8(u32(100+n*20), u32(2000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 10, 10, info);return state;}
template<> inline UndergroundState fixture<UndergroundState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=UndergroundState(u32(100+n*20), u32(2000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 10, 10, 0, 0, 1, info);return state;}
template<> inline WildGeneratorState fixture<WildGeneratorState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildGeneratorState(u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, info);return state;}
template<> inline WildGeneratorState4 fixture<WildGeneratorState4>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildGeneratorState4(u16(800+n*82), u32(100+((n+1)%4)*20), u32(100+n*20), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, info);return state;}
template<> inline WildSearcherState fixture<WildSearcherState>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildSearcherState(u32(0x12340000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, info);return state;}
template<> inline WildSearcherState4 fixture<WildSearcherState4>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildSearcherState4(u32(0x12340000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, info);state.setAdvances(100+n*20);return state;}
template<> inline WildState5 fixture<WildState5>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildState5(u16(800+n*82), 0, 0, false, false, u32(100+n*20), u32(n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, info);return state;}
template<> inline WildState8 fixture<WildState8>(int n) { auto *info=PersonalLoader::getPersonal(Game::Diamond,1);auto state=WildState8(u32(100+n*20), u32(2000+n), u32(1000+n), fixtureIVs(n), 0, 0, 5, 0, 0, 0, 0, 1, 0, 10, 10, info);return state;}
template<> inline EggState3 fixture<EggState3>(int n){auto *i=PersonalLoader::getPersonal(Game::Emerald,1);EggState3 s(100+n*20,0,1000+n,0,0,i);s.update(n,fixtureIVs(n),{1,0,0,0,0,0},i);return s;}
template<> inline EggGeneratorState4 fixture<EggGeneratorState4>(int n){auto *i=PersonalLoader::getPersonal(Game::Diamond,1);EggGeneratorState4 s(100+n*20,1000+n,0,0,i);s.update(800+n*82,n,fixtureIVs(n),{1,0,0,0,0,0},i);return s;}
template<> inline EggSearcherState4 fixture<EggSearcherState4>(int n){return EggSearcherState4(0x12340000+n,fixture<EggGeneratorState4>(n));}
template<> inline PokeSpotState fixture<PokeSpotState>(int n){auto *i=PersonalLoader::getPersonal(Game::Emerald,1);PokeSpotState s(100+n*20,1000+n,0,0,0,1,i);s.update(n,0,5,fixtureIVs(n),i);return s;}
template<> inline PickupState fixture<PickupState>(int n){return PickupState(100+n*20,800+n*82,true,{},{},fixture<WildState5>(n));}
template<> inline PokeRadarState fixture<PokeRadarState>(int n){std::array<PokeRadarPatch,4> p{};p[0]={u8(n+1),u8(n+2),1,true,false,false,true};PokeRadarState base(800+n*82,100+n*20,p);auto s=PokeRadarState(base,fixture<WildGeneratorState4>(n));s.setDisplayedBattleAdvances(100+((n+1)%4)*20);return s;}
template<> inline SearcherState5<DreamRadarState> fixture<SearcherState5<DreamRadarState>>(int n){return SearcherState5<DreamRadarState>(DateTime(2025,1,1,12,0,n),0x12340000+n,Buttons::None,0x1234,fixture<DreamRadarState>(n));}
template<> inline SearcherState5<EggState5> fixture<SearcherState5<EggState5>>(int n){return SearcherState5<EggState5>(DateTime(2025,1,1,12,0,n),0x12340000+n,Buttons::None,0x1234,fixture<EggState5>(n));}
template<> inline SearcherState5<EventState5> fixture<SearcherState5<EventState5>>(int n){return SearcherState5<EventState5>(DateTime(2025,1,1,12,0,n),0x12340000+n,Buttons::None,0x1234,fixture<EventState5>(n));}
template<> inline SearcherState5<State5> fixture<SearcherState5<State5>>(int n){return SearcherState5<State5>(DateTime(2025,1,1,12,0,n),0x12340000+n,Buttons::None,0x1234,fixture<State5>(n));}
template<> inline SearcherState5<WildState5> fixture<SearcherState5<WildState5>>(int n){return SearcherState5<WildState5>(DateTime(2025,1,1,12,0,n),0x12340000+n,Buttons::None,0x1234,fixture<WildState5>(n));}
template<class M> void fillModel(M *model){
 using T=typename std::remove_cvref_t<decltype(model->getModel())>::value_type;
 model->clearModel();for(int n=0;n<4;++n)model->addItem(fixture<T>(n));
}
inline bool fillKnown(QAbstractItemModel *model){
 if(auto *m=qobject_cast<EggModel3*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<GameCubeGeneratorModel*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<GameCubeSearcherModel*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<PokeSpotModel*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticGeneratorModel3*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticSearcherModel3*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildGeneratorModel3*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildSearcherModel3*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EggGeneratorModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EggSearcherModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EventGeneratorModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EventSearcherModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticGeneratorModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticSearcherModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildGeneratorModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildSearcherModel4*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<AdjacentSeedsModel*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<DreamRadarGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<DreamRadarSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EggGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EggSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EventGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EventSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<HiddenGrottoGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<HiddenGrottoSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<PickupGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildGeneratorModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildSearcherModel5*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<EggModel8*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<StaticModel8*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<UndergroundModel*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<WildModel8*>(model)){fillModel(m);return true;}
 if(auto *m=qobject_cast<PokeRadarModel4*>(model)){
  m->clearModel();for(int n=0;n<4;++n){auto s=fixture<PokeRadarState>(n);
   if(m->columnCount()==27) s=PokeRadarState(s,fixture<WildSearcherState4>(n),800+n*82,1);
   m->addItem(s);
  }return true;
 }
return false;}
