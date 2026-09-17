#include <PokeFinderPlus/IVTotal/IVTotalProxyModel.hpp>
#include <Model/ModelIndexMapping.hpp>
#include <QApplication>
#include <QAbstractItemModelTester>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char *msg){if(!ok)throw std::runtime_error(msg);std::cout<<"PASS "<<msg<<std::endl;}
int main(int argc,char **argv){QApplication app(argc,argv);try{
 QStandardItemModel source(4,8);source.setHorizontalHeaderLabels({"Result","HP","Atk","Def","SpA","SpD","Spe","After"});
 const int totals[]={100,2,186,10};
 for(int r=0;r<4;++r){source.setData(source.index(r,0),r+50);int n=totals[r];for(int c=1;c<7;++c){int v=std::min(n,31);source.setData(source.index(r,c),v);n-=v;}source.setData(source.index(r,7),"tail");}
 QSortFilterProxyModel prior;prior.setSourceModel(&source);prior.sort(0,Qt::DescendingOrder);
 bool ivs=true; IVTotalProxyModel columns(&prior,"Fixture",[&]{return ivs;},nullptr);QSortFilterProxyModel sorted;sorted.setSourceModel(&columns);
 QAbstractItemModelTester ct(&columns,QAbstractItemModelTester::FailureReportingMode::Fatal);
 QAbstractItemModelTester st(&sorted,QAbstractItemModelTester::FailureReportingMode::Fatal);
 check(columns.columnCount()==9&&columns.headerData(7,Qt::Horizontal)=="IV Total"&&columns.headerData(8,Qt::Horizontal)=="After","column inserted after Spe and before remaining columns");
 sorted.sort(7);const int asc[]={2,10,100,186};for(int r=0;r<4;++r)check(sorted.index(r,7).data().toInt()==asc[r],"numeric ascending order");
 QPersistentModelIndex mark=sorted.index(2,1);QPersistentModelIndex totalmark=sorted.index(2,7);
 auto identity=ModelIndexMapping::sourceRow(mark);
 sorted.sort(7,Qt::DescendingOrder);check(sorted.index(0,7).data().toInt()==186,"numeric descending maximum 186");
 check(ModelIndexMapping::sourceRow(mark)==identity&&totalmark.data().toInt()==100,"persistent original and calculated cells survive total sorting");
 prior.sort(0);check(ModelIndexMapping::sourceRow(mark)==identity&&totalmark.data().toInt()==100,"persistent indexes survive inner proxy layout sorting");
 sorted.sort(0);check(sorted.index(0,0).data().toInt()==50,"original column sorting");
 source.setData(source.index(0,1),1);check(totalmark.data().toInt()==70,"live source data change recalculates total");
 source.insertRow(0);for(int c=1;c<7;++c)source.setData(source.index(0,c),0);check(mark.isValid()&&totalmark.data().toInt()==70,"row insertion preserves identities");
 source.removeRow(0);check(mark.isValid()&&totalmark.data().toInt()==70,"row removal preserves remaining identities");
 source.setData(source.index(0,1),"A");check(!totalmark.data().isValid(),"inheritance letter is not counted as zero");
 ivs=false;columns.refreshColumns();check(columns.columnCount()==8&&!totalmark.isValid(),"Show Stats removes derived column");
 ivs=true;columns.refreshColumns();check(columns.columnCount()==9,"IV display restores column");
 source.clear();check(!mark.isValid()&&columns.rowCount()==0,"full reset invalidates persistent annotations cleanly");
 source.setColumnCount(8);source.setHorizontalHeaderLabels({"Result","HP","Atk","Def","SpA","SpD","Spe","After"});check(columns.columnCount()==9,"schema restored after reset");
 auto *ephemeral=new QStandardItemModel(1,6);ephemeral->setHorizontalHeaderLabels({"HP","Atk","Def","SpA","SpD","Spe"});
 IVTotalProxyModel lifetime(ephemeral,"Fixture",[]{return true;},nullptr);QAbstractItemModelTester lt(&lifetime,QAbstractItemModelTester::FailureReportingMode::Fatal);
 delete ephemeral;check(lifetime.rowCount()==0&&lifetime.columnCount()==0,"source destruction clears proxy safely");
 }catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}return 0;}
