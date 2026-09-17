#include <PokeFinderPlus/PluginManager.hpp>
#include <Model/ModelIndexMapping.hpp>
#include <QApplication>
#include <QMenu>
#include <QSettings>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char *msg) { if(!ok) throw std::runtime_error(msg); std::cout<<"PASS "<<msg<<std::endl; }
int main(int argc,char **argv) {
 QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
 try {
  QStandardItemModel source(3,1); for(int r=0;r<3;++r) source.setData(source.index(r,0),3-r);
  QSortFilterProxyModel a,b; a.setSourceModel(&source);b.setSourceModel(&a);a.sort(0);b.sort(0,Qt::DescendingOrder);
  check(ModelIndexMapping::sourceRow(b.index(0,0))==source.index(0,0),"stacked proxies map view to original result");
  check(ModelIndexMapping::fromSource(source.index(2,0),&b).data().toInt()==1,"stacked proxies map original result to view");
  QWidget host;host.show();PluginManager manager(&host);QApplication::processEvents();
  if(argc>1){QAction *iv=nullptr;for(auto *act:manager.menu()->actions())if(act->text()=="IV Total")iv=act;check(iv,"IV Total detected for persistence test");
   const QString mode=argv[1];
   if(mode=="enable"){if(!iv->isChecked())iv->trigger();check(iv->isChecked(),"headless active before restart");}
   else if(mode=="disable"){check(iv->isChecked(),"headless enabled state restored after restart");iv->trigger();check(!iv->isChecked(),"headless disabled state saved");}
   else if(mode=="disabled")check(!iv->isChecked(),"headless disabled state restored after restart");
   return 0;
  }
  QAction *ann=nullptr;for(auto *act:manager.menu()->actions())if(act->text()=="Annotations")ann=act;
  check(ann,"Annotations detected");if(ann->isChecked()){ann->trigger();QApplication::processEvents();}
  for(int round=0;round<2;++round) {
   ann->trigger();QApplication::processEvents();
   QWidget *window=nullptr;for(auto *w:QApplication::topLevelWidgets())if(w->windowTitle()=="Annotations"&&w->isVisible())window=w;
   check(window&&ann->isChecked(),"Annotations opens and checks menu");
   window->close();QApplication::processEvents();QApplication::processEvents();
   check(!ann->isChecked(),"Annotations X unchecks menu");
  }
  ann->trigger();QApplication::processEvents();ann->trigger();QApplication::processEvents();
  check(!ann->isChecked(),"Annotations menu disables plugin");
  for(auto *w:QApplication::topLevelWidgets())check(w->windowTitle()!="Annotations"||!w->isVisible(),"no Annotations window after shutdown");
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
 return 0;
}
