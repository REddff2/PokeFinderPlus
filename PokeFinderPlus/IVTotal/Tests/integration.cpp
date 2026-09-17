#include "fixtures.hpp"
#include <PokeFinderPlus/PluginManager.hpp>
#include <Model/ModelIndexMapping.hpp>
#include <Form/Gen3/Eggs3.hpp>
#include <Form/Gen3/GameCube.hpp>
#include <Form/Gen3/Static3.hpp>
#include <Form/Gen3/Wild3.hpp>
#include <Form/Gen3/Tools/PokeSpot.hpp>
#include <Form/Gen4/Eggs4.hpp>
#include <Form/Gen4/Event4.hpp>
#include <Form/Gen4/Static4.hpp>
#include <Form/Gen4/Wild4.hpp>
#include <Form/Gen4/PokeRadar.hpp>
#include <Form/Gen5/DreamRadar.hpp>
#include <Form/Gen5/Eggs5.hpp>
#include <Form/Gen5/Event5.hpp>
#include <Form/Gen5/HiddenGrotto.hpp>
#include <Form/Gen5/Pickup.hpp>
#include <Form/Gen5/Static5.hpp>
#include <Form/Gen5/Wild5.hpp>
#include <Form/Gen5/Phenomenon.hpp>
#include <Form/Gen5/Tools/AdjacentSeeds.hpp>
#include <Form/Gen8/Eggs8.hpp>
#include <Form/Gen8/Event8.hpp>
#include <Form/Gen8/Static8.hpp>
#include <Form/Gen8/Wild8.hpp>
#include <Form/Gen8/Raids.hpp>
#include <Form/Gen8/Underground.hpp>
#include <Form/Util/AdvanceFinder.hpp>
#include <Core/Gen3/Profile3.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen8/Profile8.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Gen5/Tools/AdjacentSeedsCalculator.hpp>
#include <QApplication>
#include <QTableView>
#include <QCheckBox>
#include <QMenu>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QTest>
#include <QLabel>
#include <QHeaderView>
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <set>

void check(bool ok,const QString &msg){if(!ok)throw std::runtime_error(msg.toStdString());std::cout<<"PASS "<<msg.toStdString()<<std::endl;}
void pump(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QApplication::processEvents();}
int totalColumn(QAbstractItemModel *m){for(int c=0;c<m->columnCount();++c)if(m->headerData(c,Qt::Horizontal)=="IV Total")return c;return -1;}
QAbstractItemModel *root(QAbstractItemModel *m){while(auto*p=qobject_cast<QAbstractProxyModel*>(m))m=p->sourceModel();return m;}
QString snapshot(QWidget *w){QStringList s;for(auto*l:w->findChildren<QLineEdit*>())s<<l->objectName()+"="+l->text();for(auto*l:w->findChildren<QComboBox*>())s<<l->objectName()+"="+l->currentText();return s.join('|');}
QString invokeResult(QWidget *w,const char *method){
 auto before=QApplication::topLevelWidgets();check(QMetaObject::invokeMethod(w,method,Qt::DirectConnection),QString("invoke ")+method);pump();
 QStringList values;for(auto*t:QApplication::topLevelWidgets())if(!before.contains(t)&&t->isVisible()){values<<snapshot(t);t->close();}pump();
 check(!values.isEmpty(),QString(method)+" opened result window");return values.join("\n");
}
void testAction(QWidget *w,QTableView *table,const char *method){
 auto *m=table->model();auto *original=root(m);const QModelIndex target=original->index(0,0);
 auto select=[&]{table->setCurrentIndex(ModelIndexMapping::fromSource(target,m));pump();};
 m->sort(-1);select();QString before=invokeResult(w,method);
 m->sort(totalColumn(m),Qt::AscendingOrder);select();QString after=invokeResult(w,method);
 check(before==after,QString(w->metaObject()->className())+"::"+method+" same result after IV Total sort");
}
QString poketch(Eggs4 *w){QString text;QTimer timer;QObject::connect(&timer,&QTimer::timeout,[&]{for(auto*t:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(t)){text=b->text();b->accept();}});timer.start(10);check(QMetaObject::invokeMethod(w,"calcPoketch",Qt::DirectConnection),"invoke Poketch");return text;}
QString tileSnapshot(QWidget *w){QStringList values;for(auto*t:w->findChildren<QWidget*>())if(QByteArray(t->metaObject()->className()).contains("PokeRadarTile"))values<<QString::number(quintptr(t))+":"+QString::number(t->grab().toImage().cacheKey());return values.join('|');}
void trigger(QTableView *v,const QString &text){for(auto*a:v->actions())if(a->text()==text){a->trigger();pump();return;}throw std::runtime_error(("missing action "+text).toStdString());}
void verifyTable(QTableView *v,const QString &label){
 auto *m=v->model();int total=totalColumn(m);check(total>=6,label+" total column attached");
 check(m->headerData(total-1,Qt::Horizontal)=="Spe",label+" column placement");
 const int asc[]={2,10,100,186};
 for(int r=0;r<4;++r){int sum=0;for(int c=total-6;c<total;++c)sum+=m->index(r,c).data().toInt();if(m->index(r,total).data().metaType().id()!=QMetaType::Int||m->index(r,total).data().toInt()!=sum)std::cout<<"DETAIL row="<<r<<" rows="<<m->rowCount()<<" total="<<m->index(r,total).data().toString().toStdString()<<" sum="<<sum<<std::endl;check(m->index(r,total).data().metaType().id()==QMetaType::Int&&m->index(r,total).data().toInt()==sum,label+" integer arithmetic");}
 m->sort(total);for(int r=0;r<4;++r)check(m->index(r,total).data().toInt()==asc[r],label+" numeric ascending");
 QPersistentModelIndex marked=m->index(2,total-3);QModelIndex identity=ModelIndexMapping::sourceRow(marked);
 m->sort(total,Qt::DescendingOrder);check(m->index(0,total).data().toInt()==186&&ModelIndexMapping::sourceRow(marked)==identity,label+" descending and persistent cell identity");
 m->sort(0);for(int r=1;r<m->rowCount();++r){bool aok=false,bok=false;double a=m->index(r-1,0).data().toDouble(&aok),b=m->index(r,0).data().toDouble(&bok);check(aok&&bok?a<=b:m->index(r-1,0).data().toString()<=m->index(r,0).data().toString(),label+" existing column ascending order");}
 v->resize(750,250);v->scrollToBottom();v->scrollToTop();
}
int main(int argc,char **argv){QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);app.setOrganizationName("IVTotalVerification");app.setApplicationName("IVTotalVerification");
 const QString data=QDir(QCoreApplication::applicationDirPath()).filePath("data/IVTotalVerification");QDir().mkpath(data);
 QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,data);
 QFile pf(data+"/profiles.json");pf.open(QIODevice::WriteOnly);pf.write("{}");pf.close();ProfileLoader::init((data+"/profiles.json").toStdWString());
 ProfileLoader3::setProfiles({Profile3("Emerald",Game::Emerald,123,456,false),Profile3("GC",Game::Gales,123,456,false)});
 Profile4 profile4("Diamond",Game::Diamond,123,456,true);ProfileLoader4::setProfiles({profile4});
 Profile5 profile5("Black",Game::Black,123,456,"","",0x123456789ABC,{true,false,false,false,false,false,false,false,false},0x60,6,5,false,0xC7F,0xC7F,false,false,DSType::DS,Language::English);
 Profile5 profile52("Black2",Game::Black2,123,456,"","",0x123456789ABC,{true,false,false,false,false,false,false,false,false},0x60,6,5,false,0xC7F,0xC7F,false,false,DSType::DS,Language::English);
 ProfileLoader5::setProfiles({profile5,profile52});ProfileLoader8::setProfiles({Profile8("BD",Game::BD,123,456,true,false,false),Profile8("Sword",Game::Sword,123,456,true,false,false)});
 Q_INIT_RESOURCE(resources);Translator::init("en");app.setStyle("fusion");
 try{
 QWidget host;host.show();PluginManager manager(&host);pump();QAction *iv=nullptr,*ann=nullptr;
 for(auto*a:manager.menu()->actions()){if(a->text()=="IV Total")iv=a;if(a->text()=="Annotations")ann=a;}
 check(iv&&ann,"both TEST plugins detected");if(ann->isChecked())ann->trigger();if(iv->isChecked())iv->trigger();pump();
 std::vector<std::pair<QString,std::function<QWidget*()>>> forms={
 {"Eggs3",[]{return new Eggs3;}},{"GameCube",[]{return new GameCube;}},{"PokeSpot",[]{return new PokeSpot;}},{"Static3",[]{return new Static3;}},{"Wild3",[]{return new Wild3;}},
 {"Eggs4",[]{return new Eggs4;}},{"Event4",[]{return new Event4;}},{"Static4",[]{return new Static4;}},{"Wild4",[]{return new Wild4;}},{"PokeRadar",[]{return new PokeRadar;}},
 {"AdjacentSeeds",[]{return new AdjacentSeeds;}},{"DreamRadar",[]{return new DreamRadar;}},{"Eggs5",[]{return new Eggs5;}},{"Event5",[]{return new Event5;}},{"HiddenGrotto",[]{return new HiddenGrotto;}},{"Pickup",[]{return new Pickup;}},{"Static5",[]{return new Static5;}},{"Wild5",[]{return new Wild5;}},{"Phenomenon",[]{return new Phenomenon;}},
 {"Eggs8",[]{return new Eggs8;}},{"Static8",[]{return new Static8;}},{"Event8",[]{return new Event8;}},{"Raids",[]{return new Raids;}},{"Underground",[]{return new Underground;}},{"Wild8",[]{return new Wild8;}}};
 std::set<std::string> classes;int tableCount=0,advanceCount=0;
 for(auto &[name,make]:forms){
  std::cout<<"BEGIN "<<name.toStdString()<<std::endl;
  if(!iv->isChecked()&&tableCount)iv->trigger();pump(); // later-created windows while active
  QPointer<QWidget> window=make();window->show();pump();
  std::vector<std::pair<QPointer<QTableView>,QAbstractItemModel*>> views;
  for(auto*v:window->findChildren<QTableView*>()){auto*base=root(v->model());if(base&&fillKnown(base)){views.push_back({v,base});classes.insert(base->metaObject()->className());}}
  if(!iv->isChecked()){int topCount=QApplication::topLevelWidgets().size();iv->trigger();pump();check(iv->isChecked()&&QApplication::topLevelWidgets().size()==topCount,"headless activation creates no window");}
  pump();
  for(auto &[vp,base]:views){auto*v=vp.data();QString label=name+"/"+base->metaObject()->className();verifyTable(v,label);++tableCount;
   if(name=="Eggs4"&&qobject_cast<EggGeneratorModel4*>(base)){
    v->model()->sort(-1);v->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),v->model()));QString before=poketch(static_cast<Eggs4*>(window.data()));
    v->model()->sort(totalColumn(v->model()));v->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),v->model()));QString after=poketch(static_cast<Eggs4*>(window.data()));
    check(!before.isEmpty()&&before==after&&after.contains("7"),"Eggs4::calcPoketch same original advances after sort");
   }
   bool search=QByteArray(base->metaObject()->className()).contains("Searcher");if(name=="PokeRadar")search=base->columnCount()==27;
   if(search&&(name=="Static3"||name=="Wild3"||name=="Eggs4"||name=="Event4"||name=="Static4"||name=="Wild4"||name=="PokeRadar"))testAction(window,v,"seedToTime");
   if(search&&(name=="Static5"||name=="Wild5"||name=="Phenomenon"||name=="HiddenGrotto"))testAction(window,v,"openAdjacentSeeds");
   if(name=="AdjacentSeeds"){
    auto*line=window->findChild<QLineEdit*>("lineEditPreview");auto*am=qobject_cast<AdjacentSeedsModel*>(base);
    for(int n=0;n<2;++n){v->model()->sort(n?totalColumn(v->model()):-1);v->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),v->model()));pump();
     auto&s=am->getItem(0);check(line->text()==QString::fromStdString(AdjacentSeedsCalculator::previewPRNG(s.getSeed(),s.getPIDAdvance(),25,true)),"AdjacentSeeds::updatePreview selection connection and result preserved");}
   }
   if(auto *egg=qobject_cast<EggGeneratorModel4*>(base)){
    egg->setShowInheritance(true);int tc=totalColumn(v->model());check(!v->model()->index(0,tc).data().isValid(),"actual Egg inheritance values leave total blank");egg->setShowInheritance(false);
    egg->setVersion(Game::HeartGold);check(totalColumn(v->model())>=6&&v->model()->headerData(totalColumn(v->model())-1,Qt::Horizontal)=="Spe","game-dependent IV block moves safely");egg->setVersion(Game::Diamond);
   }
   if(name=="PokeRadar"){
    if(!search){for(int n=0;n<2;++n){v->model()->sort(n?totalColumn(v->model()):-1);v->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),v->model()));trigger(v,"Jump to Battle Adv");check(ModelIndexMapping::sourceRow(v->currentIndex()).row()==1,"PokeRadar::jumpToBattleAdv selects same source result");}}
    const QStringList actions=search?QStringList{"Mark Patches"}:QStringList{"Mark Battle Patches","Mark Manual Patches"};
    for(auto &action:actions){QList<QImage> before;for(int n=0;n<2;++n){v->model()->sort(n?totalColumn(v->model()):-1);v->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),v->model()));trigger(v,action);
      QList<QImage> images;for(auto*t:window->findChildren<QWidget*>())if(QByteArray(t->metaObject()->className()).contains("PokeRadarTile"))images<<t->grab().toImage();
      if(n==0)before=images;else check(!images.isEmpty()&&images==before,"PokeRadar "+action+" same marked tiles after sorting");}}
   }
   if(!search&&(name=="Eggs4"||name=="Event4"||name=="Static4"||name=="Wild4"||name=="PokeRadar"||name=="Eggs5"||name=="Event5"||name=="Static5"||name=="Wild5"||name=="Phenomenon"||(name=="HiddenGrotto"&&QByteArray(base->metaObject()->className())=="HiddenGrottoGeneratorModel5"))){
    auto *af=new AdvanceFinder(base,v,name.endsWith('4')||name=="PokeRadar"?static_cast<const Profile*>(&profile4):static_cast<const Profile*>(&profile5),window);af->show();pump();af->findChild<QPushButton*>("pushButtonChatotAny")->click();pump();auto*av=af->findChild<QTableView*>("tableView");verifyTable(av,label+" AdvanceFinder");
    for(int n=0;n<2;++n){av->model()->sort(n?totalColumn(av->model()):-1);v->model()->sort(totalColumn(v->model()),Qt::DescendingOrder);av->setCurrentIndex(ModelIndexMapping::fromSource(base->index(0,0),av->model()));af->findChild<QPushButton*>("pushButtonJump")->click();check(ModelIndexMapping::sourceRow(v->currentIndex())==base->index(0,0),label+" AdvanceFinder jump preserves original result");}
    delete af;pump();++advanceCount;
   }
   if(name=="Static3"&&qobject_cast<StaticGeneratorModel3*>(base)){
    window->findChild<QLineEdit*>("textBoxGeneratorSeed")->setText("12345678");
    window->findChild<QLineEdit*>("textBoxGeneratorInitialAdvances")->setText("0");
    window->findChild<QLineEdit*>("textBoxGeneratorMaxAdvances")->setText("30");
    for(int batch=0;batch<2;++batch){window->findChild<QLineEdit*>("textBoxGeneratorSeed")->setText(batch?"87654321":"12345678");window->findChild<QPushButton*>("pushButtonGenerate")->click();pump();
     auto*m=v->model();int tc=totalColumn(m);check(m->rowCount()>10,"Static3 real RNG generation produces results");
     m->sort(tc,Qt::DescendingOrder);int previous=187;for(int r=0;r<m->rowCount();++r){int sum=0;for(int c=tc-6;c<tc;++c)sum+=m->index(r,c).data().toInt();check(sum==m->index(r,tc).data().toInt()&&sum<=previous,"Static3 live generated arithmetic and descending sort");previous=sum;}
    }
   }
   if(name=="AdjacentSeeds"){
    window->findChild<QLineEdit*>("textBoxMinIVAdvance")->setText("0");window->findChild<QLineEdit*>("textBoxMaxIVAdvance")->setText("1");window->findChild<QSpinBox*>("spinBoxSeconds")->setValue(1);
    auto *am=qobject_cast<AdjacentSeedsModel*>(base);
    for(int pass=0;pass<2;++pass){v->model()->sort(pass?totalColumn(v->model()):-1,Qt::DescendingOrder);window->findChild<QPushButton*>("pushButtonGenerate")->click();pump();
     check(am->rowCount()>0,"AdjacentSeeds live calculator results");const auto current=ModelIndexMapping::sourceRow(v->currentIndex());
     check(current.isValid()&&am->getItem(current.row()).isTarget(),"AdjacentSeeds::generate selects actual target through sorted proxies");
    }
   }
   // A new batch must refresh totals, without accumulating extra proxies/columns.
   fillKnown(base);check(v->model()->rowCount()==4,label+" new result batch");
  }
  // Show Stats uses the same headers: every applicable derived column must disappear.
  for(auto*c:window->findChildren<QCheckBox*>("checkBoxShowStats"))c->setChecked(true);pump();
  for(auto &[v,base]:views)if(name!="AdjacentSeeds")check(totalColumn(v->model())<0,name+" stats mode excluded");
  for(auto*c:window->findChildren<QCheckBox*>("checkBoxShowStats"))c->setChecked(false);pump();
  for(auto &[v,base]:views)check(totalColumn(v->model())>=6,name+" IV mode restored");
  iv->trigger();pump();check(!iv->isChecked(),name+" menu disabled");for(auto &[v,base]:views)check(totalColumn(v->model())<0&&root(v->model())==base,name+" original model restored");
  iv->trigger();pump();for(auto &[v,base]:views)check(totalColumn(v->model())>=6,name+" re-enabled");
  delete window.data();pump(); // windows destroyed while active
 }
 check(classes.size()==35&&tableCount==41,"all 35 model classes and 41 main tables covered");check(advanceCount==11,"all 11 shared IV Advance Finder views covered");
 {QWidget owner;QTableView table(&owner);QStandardItemModel unrelated(2,2,&table);table.setModel(&unrelated);owner.show();table.show();pump();table.viewport()->repaint();pump();check(totalColumn(table.model())<0,"unrelated table excluded");
  auto *replacement=new AdjacentSeedsModel(&table);fillModel(replacement);table.setModel(replacement);table.viewport()->repaint();pump();table.viewport()->repaint();pump();check(totalColumn(table.model())>=6,"replacement model discovered on existing visible table");
  delete replacement;pump();iv->trigger();pump();check(!iv->isChecked(),"source destruction followed by shutdown is safe");iv->trigger();pump();}
 ann->trigger();pump();QWidget *aw=nullptr;for(auto*w:QApplication::topLevelWidgets())if(w->windowTitle()=="Annotations"&&w->isVisible())aw=w;check(aw&&ann->isChecked()&&iv->isChecked(),"Annotations window coexists with active headless IV Total");aw->close();pump();check(!ann->isChecked()&&iv->isChecked(),"Annotations X leaves IV Total active");
 iv->trigger();pump();check(!iv->isChecked(),"final IV Total shutdown");
 std::cout<<"INTEGRATION COMPLETE models="<<classes.size()<<" tables="<<tableCount<<" AdvanceFinder="<<advanceCount<<std::endl;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}return 0;}
