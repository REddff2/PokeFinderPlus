// Temporary probe of the real frozen TEST executable; never distributed.
#include <PokeFinderPlus/PluginApi.hpp>
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QCompleter>
#include <QAbstractItemView>
#include <QDialog>
#include <QFile>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QTimer>
#include <stdexcept>

namespace {
QFile report; QString directory; int rowsChecked=0,controlsChecked=0,ambiguousRows=0; QSet<QComboBox *> seen;
void require(bool ok,const QString &message) {
    QTextStream out(&report);out<<(ok?"PASS ":"FAIL ")<<message<<'\n';out.flush();report.flush();
    if(!ok)throw std::runtime_error(message.toStdString());
}
void settle(){for(int n=0;n<6;++n)QApplication::processEvents();}
QWidget *find(const char *name) {for(auto *w:QApplication::topLevelWidgets())if(QByteArray(w->metaObject()->className())==name)return w;return nullptr;}
QWidget *open(QWidget *host,const char *method,const char *name) {
    require(QMetaObject::invokeMethod(host,method,Qt::DirectConnection),QString("open ")+name);settle();
    auto *w=find(name);require(w,QString("real window ")+name);return w;
}
QComboBox *combo(QObject *w,const QString &name) {auto *c=w->findChild<QComboBox *>(name);if(!c)throw std::runtime_error(("Missing "+name).toStdString());return c;}
QIcon icon(QComboBox *c,int row){return c->itemData(row,Qt::DecorationRole).value<QIcon>();}
QAction *action(QWidget *host,const QString &name){for(auto *a:host->findChildren<QAction *>())if(a->text()==name)return a;throw std::runtime_error("missing plugin action");}
void enable(QAction *a,bool on){if(a->isChecked()!=on)a->trigger();settle();require(a->isChecked()==on,a->text()+" active="+QString::number(on));}
void verify(QComboBox *c,bool sentinel=false,bool radarSlot=false) {
    if(!seen.contains(c)){seen.insert(c);++controlsChecked;}
    for(int row=0;row<c->count();++row) {
        const auto text=c->itemText(row);const auto image=icon(c,row);
        if(sentinel && (text=="-" || text=="None")) {if(!image.isNull())require(false,"sentinel decorated");continue;}
        if(radarSlot && image.isNull()) {
            auto *grid=qobject_cast<QGridLayout *>(c->parentWidget()->layout());
            auto *choices=qobject_cast<QComboBox *>(grid->itemAtPosition(1,1)->widget());QSet<int> forms;
            const QString name=text.section(": ",1);
            for(int r=0;r<choices->count();++r)if(choices->itemText(r).startsWith(name+" ("))forms.insert(choices->itemData(r).toInt()>>11);
            if(forms.size()>1){++ambiguousRows;continue;}
        }
        if(image.isNull() || image.pixmap(34,28).isNull()) {
            if(c->objectName().isEmpty())for(auto *sibling:c->parent()->findChildren<QComboBox *>()) {
                QTextStream out(&report);out<<"CONTEXT ";for(int r=0;r<sibling->count();++r)out<<sibling->itemText(r)<<"="<<sibling->itemData(r).toString()<<"; ";out<<'\n';
            }
            require(false,QString("missing sprite %1/%2 row %3: %4, role %5 type %6")
            .arg(c->window()->metaObject()->className(),c->objectName()).arg(row).arg(text,c->itemData(row).toString(),c->itemData(row).typeName()));
        }
        ++rowsChecked;
    }
}
void profiles(QWidget *form,const std::function<void()> &test) {
    auto *p=combo(form,"comboBoxProfiles");
    for(int n=0;n<p->count();++n){p->setCurrentIndex(n);settle();test();}
}
QComboBox *at(QGridLayout *g,int row,int col){auto *i=g->itemAtPosition(row,col);return i?qobject_cast<QComboBox *>(i->widget()):nullptr;}
QImage paint(QAbstractItemView *v,int row) {
    QImage image(260,38,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);
    QStyleOptionViewItem option;option.initFrom(v);option.widget=v;option.rect=image.rect();option.decorationSize=QSize(34,28);
    v->itemDelegate()->paint(&p,option,v->model()->index(row,0));return image;
}
}
void runBroadChecks(QWidget *host) {
    directory=PokeFinderPlus::pluginDataDirectory("SlotSprites")+"/verification/broad";QDir().mkpath(directory);
    report.setFileName(directory+"/broad.log");if(!report.open(QIODevice::WriteOnly))throw std::runtime_error("broad log");
    auto *sprites=action(host,"Slot Sprites");
    enable(sprites,true);enable(action(host,"Annotations"),true);enable(action(host,"IV Total"),true);
    for(const auto &definition: QList<QStringList>{{"openStatic3","Static3"},{"openStatic4","Static4"},{"openStatic5","Static5"},{"openStatic8","Static8"},{"openGameCube","GameCube"}}) {
        auto *form=open(host,definition[0].toLatin1(),definition[1].toLatin1());int before=rowsChecked;
        profiles(form,[&]{
            for(const auto &side:definition[1]=="Static8"?QStringList{QString()}:QStringList{"Generator","Searcher"}) {
                auto *category=combo(form,"comboBox"+side+"Category"),*pokemon=combo(form,"comboBox"+side+"Pokemon");
                for(int n=0;n<category->count();++n){category->setCurrentIndex(n);settle();verify(pokemon);}
            }
        });
        require(rowsChecked>before,definition[1]+" all profiles/categories/both selectors: "+QString::number(rowsChecked-before)+" rows");
        form->grab().save(directory+"/"+definition[1]+".png");
    }
    auto *dream=open(host,"openDreamRadar","DreamRadar");int before=rowsChecked;
    profiles(dream,[&]{for(const auto &side:{QString("Generator"),QString("Searcher")})for(int n=1;n<=6;++n)verify(combo(dream,"comboBox"+side+"Specie"+QString::number(n)),true);});
    require(rowsChecked>before,"Dream Radar all 12 selectors/both games: "+QString::number(rowsChecked-before)+" rows");
    dream->grab().save(directory+"/DreamRadar.png");
    auto *grotto=open(host,"openHiddenGrotto","HiddenGrotto");before=rowsChecked;
    profiles(grotto,[&]{for(const auto &side:{QString("Generator"),QString("Searcher")})for(const auto &tab:{QString("Pokemon"),QString("Grotto")}) {
        auto *location=combo(grotto,"comboBox"+tab+side+"Location"),*pokemon=combo(grotto,"comboBox"+tab+side+"Pokemon");
        for(int n=0;n<location->count();++n){location->setCurrentIndex(n);settle();verify(pokemon,true);}
    }});
    require(rowsChecked>before,"Hidden Grotto all locations/both games/all four selectors: "+QString::number(rowsChecked-before)+" rows");
    auto *radar=open(host,"openPokeRadar","PokeRadar");int grids=0;before=rowsChecked;
    profiles(radar,[&]{for(auto *g:radar->findChildren<QGridLayout *>())if(at(g,0,1) && QByteArray(at(g,0,1)->metaObject()->className())=="ComboBoxProxy" && at(g,5,1)) {
        ++grids;auto *location=at(g,0,1);
        for(int n=0;n<location->count();++n){location->setCurrentIndex(n);settle();verify(at(g,1,1),true);verify(at(g,5,1),false,true);
            verify(at(g,4,1),true);verify(at(g,4,2),true);}
    }});
    require(grids>=2 && rowsChecked>before,"PokeRadar both Settings grids/all locations/profiles/species/replacements/chain slots: "+QString::number(rowsChecked-before)+" rows");
    radar->grab().save(directory+"/PokeRadar.png");
    auto *raids=open(host,"openRaids","Raids");before=rowsChecked;
    profiles(raids,[&]{auto *location=combo(raids,"comboBoxLocation"),*den=combo(raids,"comboBoxDen"),*rarity=combo(raids,"comboBoxRarity");
        for(int n=0;n<location->count();++n){location->setCurrentIndex(n);settle();
            for(int d=0;d<den->count();++d){den->setCurrentIndex(d);settle();
                for(int r=0;r<(n==3?1:rarity->count());++r){rarity->setCurrentIndex(r);settle();verify(combo(raids,"comboBoxSpecies"));}
            }
        }
    });
    require(rowsChecked>before,"Raids all profiles/regions/dens/rarities/69 events: "+QString::number(rowsChecked-before)+" rows");
    raids->grab().save(directory+"/Raids.png");
    // Revisit the previously covered forms, including those not opened in the earlier milestone.
    for(const auto &definition:QList<QStringList>{{"openWild3","Wild3"},{"openWild4","Wild4"},{"openWild5","Wild5"},{"openWild8","Wild8"},
        {"openPokeSpot","PokeSpot"},{"openPhenomenon","Phenomenon"},{"openPickup","Pickup"},
        {"openEvent4","Event4"},{"openEvent5","Event5"},{"openEvent8","Event8"},
        {"openEgg3","Eggs3"},{"openEgg4","Eggs4"},{"openEgg5","Eggs5"},{"openEgg8","Eggs8"},
        {"openGameCubeSeedFinder","GameCubeSeedFinder"},{"openSIDFromChainedShiny","ChainedSID"},{"openEncounterLookup","EncounterLookup"}}) {
        auto *form=open(host,definition[0].toLatin1(),definition[1].toLatin1());before=rowsChecked;
        for(auto *c:form->findChildren<QComboBox *>()) {
            const auto name=c->objectName();
            if(name=="comboBoxGeneratorPokemon" || name=="comboBoxSearcherPokemon" || name=="comboBoxPokemon"
                || name=="comboBoxGeneratorSpecies" || name=="comboBoxSearcherSpecies" || name=="comboBoxSpecies" || name=="comboBoxEggSpecie"
                || name=="comboBoxGalesYourLead" || name=="comboBoxGalesEnemyLead" || name=="comboBoxColoPartyLead")verify(c,true);
            if(name=="comboBoxParentAGender" || name=="comboBoxParentBGender") {
                const int ditto=c->findText("Ditto");if(ditto>=0)require(!icon(c,ditto).isNull(),definition[1]+" parent Ditto choice");
            }
        }
        require(rowsChecked>before,definition[1]+" existing coverage: "+QString::number(rowsChecked-before)+" rows");
    }
    auto *calculator=find("IVCalculator");auto *species=combo(calculator,"comboBoxPokemon");auto *forms=combo(calculator,"comboBoxAltForm");before=rowsChecked;
    auto *games=combo(calculator,"comboBoxGame");
    for(int g=0;g<games->count();++g){games->setCurrentIndex(g);settle();verify(species);
        for(int n=0;n<species->count();++n){species->setCurrentIndex(n);settle();verify(forms);}}
    require(rowsChecked>before,"IV Calculator every available species/form selector: "+QString::number(rowsChecked-before)+" rows");
    auto *manager=open(host,"openProfileManager4","ProfileManager4");bool checkedUnown=false;QString dialogError;
    QTimer::singleShot(50,qApp,[&]{auto *dialog=qobject_cast<QDialog *>(find("ProfileEditor4"));
        try {
            require(dialog,"real ProfileEditor4 dialog");auto *list=combo(dialog,"checkListUnownDiscovered");auto *nativeModel=list->model();
            enable(sprites,false);auto *nativeDelegate=list->view()->itemDelegate();const auto oldSize=list->view()->iconSize();
            QList<QImage> plain;for(int r=0;r<26;++r)plain.append(paint(list->view(),r));enable(sprites,true);
            for(int r=0;r<26;++r)require(paint(list->view(),r)!=plain[r],"Unown form "+QString(QChar('A'+r))+" paints in checklist");
            list->showPopup();settle();list->view()->grab().save(directory+"/Unown-checklist.png");list->hidePopup();
            enable(sprites,false);require(list->model()==nativeModel && list->view()->itemDelegate()==nativeDelegate && list->view()->iconSize()==oldSize,"Unown disable restores original model/delegate/size");
            for(int r=0;r<26;++r)require(paint(list->view(),r)==plain[r],"Unown disable restores pixels "+QString::number(r));
            enable(sprites,true);checkedUnown=true;
        }catch(const std::exception &e){dialogError=e.what();}if(dialog)dialog->reject();
    });
    require(QMetaObject::invokeMethod(manager,"create",Qt::DirectConnection),"open real profile editor");settle();require(checkedUnown,"Unown real checklist lifecycle "+dialogError);
    // Snapshot all ordinary combos in all the real open forms, then verify reversible decoration.
    enable(sprites,false);
    struct Snapshot {QPointer<QComboBox> c;QAbstractItemModel *model;QAbstractItemDelegate *delegate;QSize size;QVariantList roles;QStringList texts;};
    QList<Snapshot> snapshots;QJsonArray inventory;
    for(auto *w:QApplication::allWidgets())if(auto *c=qobject_cast<QComboBox *>(w)) {
        Snapshot s{c,c->model(),c->itemDelegate(),c->iconSize(),{}, {}};
        for(int r=0;r<c->count();++r){s.roles.append(c->itemData(r,Qt::DecorationRole));s.texts.append(c->itemText(r));}snapshots.append(s);
    }
    enable(sprites,true);int decorated=0;
    for(const auto &s:snapshots)if(s.c){int count=0;for(int r=0;r<s.c->count();++r)count+=!icon(s.c,r).isNull();decorated+=count;
        inventory.append(QJsonObject{{"form",s.c->window()->metaObject()->className()},{"name",s.c->objectName()},
            {"class",s.c->metaObject()->className()},{"rows",s.c->count()},{"icons",count},{"sample",s.c->itemText(0)}});}
    enable(sprites,false);
    for(const auto &s:snapshots)if(s.c){bool same=s.c->model()==s.model && s.c->itemDelegate()==s.delegate && s.c->iconSize()==s.size;
        for(int r=0;r<s.c->count();++r)same&=s.c->itemText(r)==s.texts[r] && s.c->itemData(r,Qt::DecorationRole)==s.roles[r];
        if(!same)require(false,"restoration failed "+s.c->objectName());}
    require(true,QString("all %1 controls restore model/delegate/icon size/role/text after disable").arg(snapshots.size()));
    enable(sprites,true);int after=0;for(const auto &s:snapshots)if(s.c)for(int r=0;r<s.c->count();++r)after+=!icon(s.c,r).isNull();
    require(after==decorated && decorated>1000,QString("re-enable restores %1 decorated rows across real windows").arg(after));
    QFile file(directory+"/runtime-inventory.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(inventory).toJson());
    require(action(host,"Annotations")->isChecked() && action(host,"IV Total")->isChecked(),"all three plugins remain active");
    require(true,QString("BROAD CHECKS PASSED: %1 row checks, %2 distinct Pokemon controls, %3 ambiguous Radar slot rows safely skipped").arg(rowsChecked).arg(controlsChecked).arg(ambiguousRows));report.close();
}
