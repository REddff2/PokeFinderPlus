// Developer tool: freezes the exact host encounter tables into plugin resources.
#include <Core/Gen3/Encounters3.hpp>
#include <Core/Gen4/Encounters4.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen8/Encounters8.hpp>
#include <Core/Gen8/Den.hpp>
#include <Core/Gen3/ShadowTemplate.hpp>
#include <Core/Gen5/DreamRadarTemplate.hpp>
#include <Core/Gen3/StaticTemplate3.hpp>
#include <Core/Gen4/StaticTemplate4.hpp>
#include <Core/Gen5/StaticTemplate5.hpp>
#include <Core/Gen8/StaticTemplate8.hpp>
#include <Core/Gen5/HiddenGrottoArea.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Shiny.hpp>
#include <Core/Parents/PersonalLoader.hpp>
#include <Core/Parents/PersonalInfo.hpp>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
template<class T> QJsonArray identity(const T &t)
{
    return {t.getSpecie(),t.getForm(),t.getGender()==0 || t.getGender()==1 ? int(t.getGender()) : -1,
        t.getShiny()==Shiny::Always || t.getShiny()==Shiny::Star || t.getShiny()==Shiny::Square};
}
template<class F> QJsonArray categories(int total,F get)
{
    QJsonArray result;
    for(int c=0;c<total;++c){int size=0;auto *templates=get(c,&size);QJsonArray rows;for(int i=0;i<size;++i)rows.append(identity(templates[i]));result.append(rows);}
    return result;
}
template<class T> QJsonArray raids(const T &den,Game game)
{
    QJsonArray result;
    for(const auto &raid:den.getRaids(game))
    {
        if(!raid.getSpecie())break;
        auto value=identity(raid);value[2]=raid.getGender()==1 || raid.getGender()==2 ? int(raid.getGender())-1 : -1;
        value.append(raid.getGigantamax());value.append(QString::fromStdString(raid.getStarDisplay()));
        result.append(value);
    }
    return result;
}
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);QJsonObject data;
    data["Static3"]=categories(10,Encounters3::getStaticEncounters);
    data["Static4"]=categories(8,Encounters4::getStaticEncounters);
    data["Static5"]=categories(9,Encounters5::getStaticEncounters);
    data["Static8"]=categories(9,Encounters8::getStaticEncounters);
    int count=0;auto *shadow=Encounters3::getShadowTeams(&count);QJsonArray shadows;
    for(int i=0;i<count;++i)shadows.append(identity(shadow[i]));data["Shadow"]=shadows;
    auto *dream=Encounters5::getDreamRadarEncounters(&count);QJsonArray dreams;
    for(int i=0;i<count;++i)dreams.append(identity(dream[i]));data["DreamRadar"]=dreams;
    QJsonObject grotto;
    for(auto game:{Game::Black2,Game::White2})
    {
        QJsonArray areas;
        for(const auto &area:Encounters5::getHiddenGrottoEncounters())
        {
            QJsonArray rows;for(int n=0;n<12;++n)rows.append(QJsonArray{area.getPokemon(n/3,n%3,game).getSpecie(),0,-1,false});
            areas.append(rows);
        }
        grotto[game==Game::Black2?"Black 2":"White 2"]=areas;
    }
    data["Grotto"]=grotto;
    QJsonObject dens,events;
    for(auto game:{Game::Sword,Game::Shield})
    {
        QJsonArray normal,promoted;
        for(int n=0;n<276;++n)
        {
            if(n==16){normal.append(QJsonArray{});continue;}
            normal.append(QJsonArray{raids(*Encounters8::getDen(n,0),game),raids(*Encounters8::getDen(n,1),game)});
        }
        for(int n=0;n<69;++n)promoted.append(raids(*Encounters8::getDenEvent(n),game));
        dens[game==Game::Sword?"Sword":"Shield"]=normal;events[game==Game::Sword?"Sword":"Shield"]=promoted;
    }
    data["Dens"]=dens;data["Events"]=events;
    QJsonObject personal;
    for(auto game:{Game::Emerald,Game::Diamond,Game::HeartGold,Game::Black,Game::Black2,Game::Sword,Game::BD}) {
        const auto *info=PersonalLoader::getPersonal(game);QJsonObject counts;
        int max=game==Game::Sword?898:game==Game::Emerald?386:game==Game::Black || game==Game::Black2?649:493;
        for(int s=1;s<=max;++s)if(info[s].getPresent())counts[QString::number(s)]=info[s].getFormCount();
        personal[QString::number(toInt(game))]=counts;
    }
    data["PersonalForms"]=personal;
    QFile out(app.arguments().value(1));if(!out.open(QIODevice::WriteOnly))return 1;
    out.write(QJsonDocument(data).toJson(QJsonDocument::Compact));std::cout<<"Exported frozen host control identities"<<std::endl;
}
