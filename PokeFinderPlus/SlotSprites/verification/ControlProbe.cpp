#include <PokeFinderPlus/PluginApi.hpp>
#include <QApplication>
#include <QAction>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QElapsedTimer>
#include <stdexcept>

namespace
{
QFile output;
QString directory;
void check(bool ok,const QString &message)
{
    QTextStream stream(&output);stream<<(ok?"PASS ":"FAIL ")<<message<<'\n';stream.flush();output.flush();
    if(!ok)throw std::runtime_error(message.toStdString());
}
void settle(){for(int i=0;i<8;++i)QApplication::processEvents();}
QAction *action(QWidget *host,const QString &text)
{
    for(auto *a:host->findChildren<QAction *>())if(a->text()==text)return a;
    throw std::runtime_error("Plugin menu action missing");
}
void enable(QAction *a,bool state){if(a->isChecked()!=state)a->trigger();settle();check(a->isChecked()==state,a->text()+" active="+QString::number(state));}
QWidget *window(const char *className)
{
    for(auto *w:QApplication::topLevelWidgets())if(QByteArray(w->metaObject()->className())==className)return w;
    return nullptr;
}
QWidget *open(QWidget *host,const char *method,const char *className)
{
    check(QMetaObject::invokeMethod(host,method,Qt::DirectConnection),QString("open ")+className);settle();
    auto *w=window(className);check(w,QString("actual window exists: ")+className);return w;
}
QComboBox *combo(QWidget *form,const char *name)
{
    auto *c=form->findChild<QComboBox *>(name);check(c,QString("actual control exists: ")+name);return c;
}
QIcon rowIcon(QComboBox *c,int row){return c->itemData(row,Qt::DecorationRole).value<QIcon>();}
void key(QWidget *widget,int code,QString text={},Qt::KeyboardModifiers modifiers=Qt::NoModifier)
{
    QKeyEvent press(QEvent::KeyPress,code,modifiers,text),release(QEvent::KeyRelease,code,modifiers,text);
    QApplication::sendEvent(widget,&press);QApplication::sendEvent(widget,&release);settle();
}
int colored(const QImage &image)
{
    int pixels=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
    {
        auto color=image.pixelColor(x,y);
        if(color.alpha()>0 && color.saturation()>90 && color.value()>70)++pixels;
    }
    return pixels;
}
QImage rowImage(QAbstractItemView *view,int row)
{
    QImage image(260,38,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
    QPainter painter(&image);QStyleOptionViewItem option;option.initFrom(view);option.widget=view;option.rect=image.rect();
    option.decorationSize=QSize(34,28);option.state&=~(QStyle::State_Selected|QStyle::State_HasFocus|QStyle::State_MouseOver);
    auto index=view->model()->index(row,0);view->itemDelegateForIndex(index)->paint(&painter,option,index);return image;
}
}
void runControlChecks(QWidget *host)
{
    directory=PokeFinderPlus::pluginDataDirectory("SlotSprites")+"/verification/controls";QDir().mkpath(directory);
    output.setFileName(directory+"/controls.log");if(!output.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open control log");
    auto *sprites=action(host,"Slot Sprites"),*iv=action(host,"IV Total"),*annotations=action(host,"Annotations");
    enable(sprites,false);enable(annotations,false);enable(iv,false);
    auto *wild=window("Wild3");check(wild,"Wild3 already open before activation");
    auto *choices=combo(wild,"comboBoxGeneratorPokemon");
    auto *location=combo(wild,"comboBoxGeneratorLocation");
    for(int row=0;row<location->count() && choices->count()<=2;++row){location->setCurrentIndex(row);settle();}
    check(choices->count()>2,"real Wild3 species choices populated");
    choices->setCurrentIndex(1);settle();
    auto *originalModel=choices->model();auto *originalDelegate=choices->itemDelegate();const auto originalSize=choices->iconSize();
    const auto originalData=choices->currentData();const auto originalText=choices->currentText();
    const auto before=choices->grab().toImage();int indexSignals=0;
    QObject observer;QObject::connect(choices,&QComboBox::currentIndexChanged,&observer,[&]{++indexSignals;});
    enable(sprites,true);
    check(indexSignals==0 && choices->currentData()==originalData && choices->currentText()==originalText,"activation preserves selection/value/text without selection signals");
    check(choices->model()==originalModel && choices->itemDelegate()==originalDelegate,"combo keeps original model and delegate");
    int decorated=0;
    for(int row=0;row<choices->count();++row)
    {
        if(choices->itemData(row).toUInt()>0)check(!rowIcon(choices,row).isNull(),QString("resolvable popup row ")+QString::number(row)),++decorated;
        else check(rowIcon(choices,row).isNull(),"dash has no sprite");
    }
    check(decorated>1,"multiple real Pokemon entries decorated");
    check(colored(choices->grab().toImage())>colored(before),"closed combo visibly paints a Pokemon sprite before unchanged text");
    choices->grab().save(directory+"/wild3-closed.png");
    choices->showPopup();settle();choices->view()->grab().save(directory+"/wild3-popup.png");
    check(colored(rowImage(choices->view(),1))>10,"native combo popup delegate paints actual sprite pixels");choices->hidePopup();settle();
    const auto firstIcon=rowIcon(choices,1).pixmap(QSize(34,28)).toImage();
    choices->setCurrentIndex(2);settle();check(choices->currentData()!=originalData,"selecting another Pokemon changes the underlying value normally");
    check(rowIcon(choices,2).pixmap(QSize(34,28)).toImage()!=firstIcon,"selected Pokemon artwork changes");
    choices->setCurrentIndex(1);settle();key(choices,Qt::Key_Down);
    check(choices->currentIndex()==2,"closed combo keyboard Down selects next Pokemon");
    const auto selected=choices->currentData();
    enable(iv,true);enable(annotations,true);check(!rowIcon(choices,2).isNull(),"combo icons coexist with both other deployed plugins");
    enable(sprites,false);
    check(choices->model()==originalModel && choices->itemDelegate()==originalDelegate && choices->iconSize()==originalSize
          && choices->currentData()==selected,"disable restores combo objects/icon size and preserves user selection");
    for(int row=0;row<choices->count();++row)check(!choices->itemData(row,Qt::DecorationRole).isValid(),"disable restores originally absent decoration role");
    enable(sprites,true);check(!rowIcon(choices,2).isNull(),"re-enable restores combo sprites");

    auto *calculator=open(host,"openIVCalculator","IVCalculator");auto *species=combo(calculator,"comboBoxPokemon");
    check(species->isEditable() && species->count()>100,"late-created editable species selector is populated");
    auto *edit=species->lineEdit();auto *completion=species->completer();
    check(!rowIcon(species,0).isNull(),"late-created IV Calculator gains sprites");
    edit->setFocus();key(edit,Qt::Key_A,{},Qt::ControlModifier);
    for(const QChar c:QString("Pika"))key(edit,c.toUpper().unicode(),QString(c));
    check(edit->text()=="Pika" && species->completer()==completion,"editable typing preserves native completer and text");
    completion->setCompletionPrefix("Pika");completion->complete();settle();
    check(completion->completionCount()>0,"native contains-search still finds Pokemon");
    check(!completion->completionModel()->index(0,0).data(Qt::DecorationRole).value<QIcon>().isNull(),"completion list retains native model sprite decoration");
    completion->popup()->grab().save(directory+"/completion-popup.png");completion->popup()->hide();
    species->setCurrentIndex(species->findData(25));settle();species->grab().save(directory+"/editable-closed.png");
    check(colored(species->grab().toImage())>10,"editable closed combo paints Pokemon icon");
    auto *game=combo(calculator,"comboBoxGame");if(game->count()>1){game->setCurrentIndex((game->currentIndex()+1)%game->count());settle();}
    check(species->count()>100 && !rowIcon(species,0).isNull(),"game change clears/repopulates model and new items gain sprites");

    auto *lookup=open(host,"openEncounterLookup","EncounterLookup");auto *lookupSpecies=combo(lookup,"comboBoxPokemon");
    check(!lookupSpecies->itemData(0).isValid() && !rowIcon(lookupSpecies,0).isNull(),"display-only Encounter Lookup uses verified localized-name mapping");
    auto *event=open(host,"openEvent8","Event8");auto *eventSpecies=combo(event,"comboBoxSpecies");
    check(!rowIcon(eventSpecies,eventSpecies->findText("Pikachu")).isNull(),"Event8 display-only species choices gain sprites");
    auto *eggs=open(host,"openEgg3","Eggs3");
    int eggCount=0;for(auto *c:eggs->findChildren<QComboBox *>("comboBoxEggSpecie"))if(c->count() && !rowIcon(c,0).isNull())++eggCount;
    check(eggCount>0,"nested EggSettings controls discovered in real egg window");

    auto *underground=open(host,"openUnderground","Underground");auto *list=combo(underground,"checkListPokemon");
    check(list->count()>0,"real Underground Pokemon checklist populated");
    const QString summary=list->currentText();auto *listModel=list->model();
    check(!list->itemData(0,Qt::DecorationRole).isValid(),"checklist model remains undecorated so summary gets no misleading icon");
    list->showPopup();settle();
    check(colored(rowImage(list->view(),0))>10,"Underground QListView popup visibly paints species sprite");
    list->view()->grab().save(directory+"/underground-popup.png");list->hidePopup();settle();
    const auto item=list->model()->index(0,0);const auto checked=item.data(Qt::CheckStateRole);
    check(QMetaObject::invokeMethod(list->view(),"pressed",Qt::DirectConnection,Q_ARG(QModelIndex,item)),"native checklist pressed event delivered");settle();
    check(item.data(Qt::CheckStateRole)!=checked,"native checklist selection toggles normally");
    const auto checkedAfter=item.data(Qt::CheckStateRole);
    enable(sprites,false);check(list->model()==listModel && item.data(Qt::CheckStateRole)==checkedAfter,"disable preserves checklist model/check state");
    check(colored(rowImage(list->view(),0))<10,"disable restores original undecorated checklist popup");
    enable(sprites,true);check(colored(rowImage(list->view(),0))>10,"re-enable restores checklist popup sprites");

    QElapsedTimer timer;timer.start();
    for(int i=0;i<300;++i)species->setCurrentIndex(i%species->count());settle();
    check(timer.elapsed()<5000,QString("300 large-combo selections remain responsive: %1 ms").arg(timer.elapsed()));
    // Isolated Qt controls inside the inspected form exercise lifecycle/identity
    // edge cases without changing the real host selector's result configuration.
    auto fixture=std::make_unique<QComboBox>(wild);fixture->setObjectName("comboBoxGeneratorPokemon");
    fixture->addItem("Pikachu",25);fixture->addItem("Unown (!)",201|(26<<11));
    fixture->addItem("Unknown Unown form",201|(31<<11));fixture->addItem("Pikachu",QString("25"));
    QPixmap marker(12,12);marker.fill(Qt::magenta);const QIcon priorIcon(marker);
    fixture->addItem(priorIcon,"Existing icon",1);fixture->show();settle();
    check(!rowIcon(fixture.get(),0).isNull() && !rowIcon(fixture.get(),1).isNull(),"numeric species and packed explicit form identities decorate");
    check(rowIcon(fixture.get(),2).isNull() && rowIcon(fixture.get(),3).isNull(),"unknown forms and non-numeric ID roles fail closed without name guessing");
    check(rowIcon(fixture.get(),4).cacheKey()==priorIcon.cacheKey(),"pre-existing model icon is preserved");
    const auto unown=rowIcon(fixture.get(),1).pixmap(QSize(34,28)).toImage();
    auto *fixtureModel=qobject_cast<QStandardItemModel *>(fixture->model());fixtureModel->sort(0,Qt::DescendingOrder);settle();
    check(rowIcon(fixture.get(),fixture->findData(201|(26<<11))).pixmap(QSize(34,28)).toImage()==unown,"model sorting preserves form-to-icon identity");
    auto *replacement=new QStandardItemModel(fixture.get());auto *replacementItem=new QStandardItem("Bulbasaur");
    replacementItem->setData(1,Qt::UserRole);replacement->appendRow(replacementItem);fixture->setModel(replacement);settle();
    check(fixture->model()==replacement && !rowIcon(fixture.get(),0).isNull(),"model replacement is detected and decorated without substituting another model");
    auto retainedIcon=rowIcon(fixture.get(),0);enable(sprites,false);
    check(!fixture->itemData(0,Qt::DecorationRole).isValid() && retainedIcon.isNull(),"disable removes decorations and retained lazy icons safely expire");
    enable(sprites,true);fixture.reset();settle();check(true,"decorated control/model destruction is safe");
    auto localized=std::make_unique<QComboBox>(lookup);localized->setObjectName("comboBoxPokemon");
    localized->addItem(QString::fromUtf8("ピカチュウ"));localized->addItem("Pikachu?");localized->addItem("Pikachu",25);
    localized->show();settle();check(!rowIcon(localized.get(),0).isNull() && rowIcon(localized.get(),1).isNull()
        && rowIcon(localized.get(),2).isNull(),"localized exact-name fallback works; fuzzy names and unexpected data contracts remain unchanged");
    localized.reset();settle();
    QJsonArray inventory;
    for(auto *w:QApplication::allWidgets())if(auto *c=qobject_cast<QComboBox *>(w))
    {
        int icons=0;for(int row=0;row<c->count();++row)icons+=!rowIcon(c,row).isNull();
        inventory.append(QJsonObject{{"form",c->window()->metaObject()->className()},{"name",c->objectName()},
            {"class",c->metaObject()->className()},{"model",c->model()->metaObject()->className()},
            {"rows",c->count()},{"icons",icons},{"sampleText",c->itemText(0)},
            {"sampleUserRole",QJsonValue::fromVariant(c->itemData(0))},{"sampleUserRolePlusOne",QJsonValue::fromVariant(c->itemData(0,Qt::UserRole+1))}});
    }
    QFile inventoryFile(directory+"/runtime-inventory.json");if(inventoryFile.open(QIODevice::WriteOnly))inventoryFile.write(QJsonDocument(inventory).toJson());
    check(true,"CONTROL CHECKS PASSED with all three deployed plugins active");output.close();
}
