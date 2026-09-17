#include "PokemonControls.hpp"
#include "SpriteCache.hpp"
#include "SpriteResolver.hpp"
#include <QAbstractProxyModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QIconEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <algorithm>
#include <functional>
#include <vector>

namespace
{
const QSize spriteSize(34,28); // Existing Pokémon bounds; table sizing is untouched.
enum class Contract {None, Packed, Species, Names, Checklist, Template, Dream, Grotto, Raid, RadarSlot, Form, Unown};
bool checklist(Contract kind){return kind==Contract::Checklist || kind==Contract::Unown;}
QObject *owner(QComboBox *combo)
{
    for(QObject *p=combo->parent();p;p=p->parent())
        if(QList<QByteArray>{"Static3","Static4","Static5","Static8","GameCube","DreamRadar","HiddenGrotto",
            "Raids","IVCalculator","EggSettings","ProfileEditor4","PokeRadar"}.contains(p->metaObject()->className()))return p;
    return combo->window();
}
QComboBox *at(QGridLayout *grid,int row,int column)
{
    const auto *item=grid?grid->itemAtPosition(row,column):nullptr;
    return item?qobject_cast<QComboBox *>(item->widget()):nullptr;
}
Contract contract(QComboBox *combo)
{
    const QString name=combo->objectName();
    for(QObject *p=combo->parent();p;p=p->parent())
    {
        const QByteArray form=p->metaObject()->className();
        const bool pair=name=="comboBoxGeneratorPokemon" || name=="comboBoxSearcherPokemon";
        if(((form=="Static3" || form=="Static4" || form=="Static5" || form=="GameCube") && pair)
            || (form=="Static8" && name=="comboBoxPokemon"))return Contract::Template;
        if(form=="DreamRadar" && (name.startsWith("comboBoxGeneratorSpecie") || name.startsWith("comboBoxSearcherSpecie"))
            && name.right(1).toInt()>=1 && name.right(1).toInt()<=6)return Contract::Dream;
        if(form=="HiddenGrotto" && (name=="comboBoxPokemonGeneratorPokemon" || name=="comboBoxPokemonSearcherPokemon"))return Contract::Grotto;
        if(form=="Raids" && name=="comboBoxSpecies")return Contract::Raid;
        if(form=="IVCalculator" && name=="comboBoxAltForm")return Contract::Form;
        if(form=="EggSettings" && (name=="comboBoxParentAGender" || name=="comboBoxParentBGender"))return Contract::Names;
        if(form=="ProfileEditor4" && name=="checkListUnownDiscovered")return Contract::Unown;
        if(form=="PokeRadar" && name.isEmpty())
        {
            // These controls have no object names. Validate the source-defined
            // Settings grid and control types, then use its semantic positions.
            auto *grid=qobject_cast<QGridLayout *>(combo->parentWidget()->layout());
            if(grid && at(grid,0,1) && QByteArray(at(grid,0,1)->metaObject()->className())=="ComboBoxProxy"
                && at(grid,1,1) && at(grid,2,1) && at(grid,5,1))
            {
                if(combo==at(grid,1,1))return Contract::Packed;
                if(combo==at(grid,4,1) || combo==at(grid,4,2))return Contract::Species;
                if(combo==at(grid,5,1))return Contract::RadarSlot;
            }
        }
        if((form=="Wild3" || form=="Wild4" || form=="Wild5" || form=="Phenomenon" || form=="Pickup") && pair) return Contract::Packed;
        if((form=="Wild8" || form=="PokeSpot") && name=="comboBoxPokemon") return Contract::Packed;
        if(form=="Wild4" && (name=="comboBoxGeneratorReplacement0" || name=="comboBoxGeneratorReplacement1"
            || name=="comboBoxSearcherReplacement0" || name=="comboBoxSearcherReplacement1")) return Contract::Species;
        if(form=="Wild8" && (name=="comboBoxReplacement0" || name=="comboBoxReplacement1")) return Contract::Species;
        if(form=="HiddenGrotto" && (name=="comboBoxGrottoGeneratorPokemon" || name=="comboBoxGrottoSearcherPokemon")) return Contract::Species;
        if((form=="IVCalculator" || form=="ChainedSID") && name=="comboBoxPokemon") return Contract::Species;
        if(form=="EggSettings" && name=="comboBoxEggSpecie") return Contract::Species;
        if(form=="EncounterLookup" && name=="comboBoxPokemon") return Contract::Names;
        if((form=="Event4" || form=="Event5") && (name=="comboBoxGeneratorSpecies" || name=="comboBoxSearcherSpecies")) return Contract::Names;
        if(form=="Event8" && name=="comboBoxSpecies") return Contract::Names;
        if(form=="GameCubeSeedFinder" && (name=="comboBoxGalesYourLead" || name=="comboBoxGalesEnemyLead" || name=="comboBoxColoPartyLead")) return Contract::Names;
        if(form=="Underground" && name=="checkListPokemon" && QByteArray(combo->metaObject()->className())=="CheckList") return Contract::Checklist;
    }
    return Contract::None;
}
struct Resources
{
    SpriteResolver &resolver;
    SpriteCache &cache;
    QJsonObject names;
    QJsonObject forms;
    QJsonObject contracts,games;
    Resources(SpriteResolver &r,SpriteCache &c):resolver(r),cache(c)
    {
        QFile file(":/slotsprites/control-names.json");
        if(file.open(QIODevice::ReadOnly)) names=QJsonDocument::fromJson(file.readAll()).object();
        QFile contractsFile(":/slotsprites/control-contracts.json"),gamesFile(":/slotsprites/control-games.json");
        if(contractsFile.open(QIODevice::ReadOnly))contracts=QJsonDocument::fromJson(contractsFile.readAll()).object();
        if(gamesFile.open(QIODevice::ReadOnly))games=QJsonDocument::fromJson(gamesFile.readAll()).object();
        QFile recipe(":/slotsprites/bootstrap.json");
        if(recipe.open(QIODevice::ReadOnly))
        {
            const auto json=QJsonDocument::fromJson(recipe.readAll()).object();
            forms=QJsonDocument::fromJson(json.value("manifestText").toString().toUtf8()).object().value("pokemon").toObject();
        }
    }
};
QJsonArray entry(QJsonArray array,int index){return index>=0 && index<array.size()?array[index].toArray():QJsonArray();}
QString profileGame(QObject *form,const Resources &resources)
{
    const auto labels=form->findChildren<QLabel *>("labelGameValue");
    if(labels.size()!=1)return {};
    return resources.games.value(labels.first()->text()).toString();
}
ResultIdentity identity(const QModelIndex &index,Contract kind,const Resources &resources,QComboBox *combo=nullptr)
{
    ResultIdentity result;
    if(kind==Contract::Template || kind==Contract::Dream || kind==Contract::Grotto || kind==Contract::Raid)
    {
        if(!combo)return {};
        QObject *form=owner(combo);QString name=combo->objectName();QJsonArray value;
        bool valid=false;int id=index.data(Qt::UserRole).toInt(&valid);
        const auto dataType=index.data(Qt::UserRole).metaType().id();
        if(kind!=Contract::Raid && (!valid || (dataType!=QMetaType::Int && dataType!=QMetaType::UInt
            && dataType!=QMetaType::UChar && dataType!=QMetaType::UShort) || id<0))return {};
        if(kind==Contract::Template)
        {
            const bool single=QByteArray(form->metaObject()->className())=="Static8";
            const QString side=single?QString():name.contains("Searcher")?"Searcher":"Generator";
            auto *category=form->findChild<QComboBox *>("comboBox"+side+"Category");if(!category)return {};
            int cat=category->currentIndex();QString key=form->metaObject()->className();
            if(key=="GameCube")
            {
                if(cat==2)value=entry(resources.contracts.value("Shadow").toArray(),id);
                else {key="Static3";cat+=8;}
            }
            if(value.isEmpty())value=entry(entry(resources.contracts.value(key).toArray(),cat),id);
        }
        else if(kind==Contract::Dream)value=entry(resources.contracts.value("DreamRadar").toArray(),id);
        else if(kind==Contract::Grotto)
        {
            auto *location=form->findChild<QComboBox *>(name.contains("Searcher")?"comboBoxPokemonSearcherLocation":"comboBoxPokemonGeneratorLocation");
            if(!location)return {};
            QModelIndex area=location->model()->index(location->currentIndex(),location->modelColumn());
            while(auto *proxy=qobject_cast<const QAbstractProxyModel *>(area.model()))area=proxy->mapToSource(area);
            value=entry(entry(resources.contracts.value("Grotto").toObject().value(profileGame(form,resources)).toArray(),area.row()),id);
        }
        else
        {
            auto *location=form->findChild<QComboBox *>("comboBoxLocation"),*den=form->findChild<QComboBox *>("comboBoxDen"),*rarity=form->findChild<QComboBox *>("comboBoxRarity");
            if(!location || !den || !rarity)return {};
            QString game=profileGame(form,resources);
            // The frozen host's default Raid profile is BD; Den::getRaids
            // explicitly uses the Shield table for every non-Sword version.
            if(game=="Brilliant Diamond" || game=="Shining Pearl")game="Shield";
            if(location->currentIndex()==3)value=entry(entry(resources.contracts.value("Events").toObject().value(game).toArray(),den->currentIndex()),index.row());
            else
            {
                bool ok=false;int denId=den->currentData().toInt(&ok);if(!ok)return {};
                value=entry(entry(entry(resources.contracts.value("Dens").toObject().value(game).toArray(),denId),rarity->currentIndex()),index.row());
            }
        }
        if(value.size()<4)return {};
        result={value[0].toInt(),value[1].toInt(),value[2].toInt(),value[3].toBool(),0};
        QString displayed=index.data().toString();
        if(kind==Contract::Raid)
        {
            if(value.size()<6)return {};
            const QString suffix=": "+value[5].toString();if(!displayed.endsWith(suffix))return {};
            displayed.chop(suffix.size());
        }
        // Guard the frozen encounter-index mapping against a changed host list.
        const auto named=resources.names.value(displayed).toArray();
        if(named.size()!=2 || named[0].toInt()!=result.species)return {};
        if(kind==Contract::Raid && value.size()>4 && value[4].toBool())result.form+=1000;
    }
    else if(kind==Contract::Form)
    {
        if(!combo)return {};
        auto *species=owner(combo)->findChild<QComboBox *>("comboBoxPokemon");if(!species)return {};
        result.species=species->currentData().toInt();result.form=index.row();
        if(result.species==493) {
            auto *game=owner(combo)->findChild<QComboBox *>("comboBoxGame");if(!game)return {};
            if(game->currentData().toUInt() & 0xf80u)result.form+=2000; // Gen4 retains the ??? type between Steel and Fire.
        }
    }
    else if(kind==Contract::Unown)
    {
        const auto name=index.data().toString();if(name.size()!=1 || name[0]<'A' || name[0]>'Z')return {};
        result.species=201;result.form=name[0].unicode()-'A';
    }
    else if(kind==Contract::RadarSlot)
    {
        if(!combo)return {};
        auto *grid=qobject_cast<QGridLayout *>(combo->parentWidget()->layout());auto *choices=at(grid,1,1);if(!choices)return {};
        bool ok=false;int slot=index.data(Qt::UserRole).toInt(&ok);const QString text=index.data().toString();
        const QString prefix=QString::number(slot)+": ";if(!ok || slot<0 || slot>11 || !text.startsWith(prefix))return {};
        const auto named=resources.names.value(text.mid(prefix.size())).toArray();if(named.size()!=2)return {};
        bool found=false;
        for(int row=0;row<choices->count();++row)
        {
            const auto candidate=identity(choices->model()->index(row,choices->modelColumn()),Contract::Packed,resources,choices);
            if(candidate.species!=named[0].toInt())continue;
            if(found && candidate.form!=result.form)return {};
            result=candidate;found=true;
        }
        if(!found)return {};
    }
    else if(kind==Contract::Names)
    {
        // These inspected controls store only display strings. Never fall back to
        // names when a numeric contract exists but its value is invalid.
        if(index.data(Qt::UserRole).isValid()) return result;
        const auto value=resources.names.value(index.data(Qt::DisplayRole).toString()).toArray();
        if(value.size()==2) {result.species=value[0].toInt();result.form=value[1].toInt();}
    }
    else
    {
        const QVariant data=index.data(kind==Contract::Checklist?Qt::UserRole+1:Qt::UserRole);
        // Strings and floating-point values are not the host's species-ID contract.
        const int type=data.metaType().id();
        if(type!=QMetaType::Int && type!=QMetaType::UInt && type!=QMetaType::UShort && type!=QMetaType::Short) return result;
        bool ok=false; const int value=data.toInt(&ok);
        if(!ok || value<=0 || value>65535) return result;
        result.species=kind==Contract::Packed ? value&0x7ff : value;
        result.form=kind==Contract::Packed ? value>>11 : 0;
    }
    if(result.species<1 || result.species>898) return {};
    // A form-specific UI choice must have an explicit mapping. The existing
    // table resolver's normal-species fallback remains unchanged for tables.
    if(!resources.forms.value(QString::number(result.species)).toObject().contains(QString::number(result.form)))return {};
    return result; // Generic selector entries do not represent a shiny or gender result.
}
class LazyIcon final : public QIconEngine
{
    std::weak_ptr<Resources> resources;
    QStringList paths;
public:
    LazyIcon(const std::shared_ptr<Resources> &r,QStringList p):resources(r),paths(std::move(p)){}
    QIconEngine *clone() const override {return new LazyIcon(*this);}
    bool isNull() override {return resources.expired() || paths.isEmpty();}
    QString key() const override {return "SlotSpritesPokemon";}
    QPixmap scaledPixmap(const QSize &size,QIcon::Mode,QIcon::State,qreal scale) override
    {
        if(auto r=resources.lock()) return r->cache.get(paths,size.boundedTo(spriteSize),scale);
        return {};
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override {return scaledPixmap(size,mode,state,1);}
    void paint(QPainter *painter,const QRect &rect,QIcon::Mode mode,QIcon::State state) override
    {
        const auto pix=scaledPixmap(rect.size(),mode,state,painter->device()->devicePixelRatioF());
        if(pix.isNull())return;
        const QSizeF logical=pix.deviceIndependentSize();
        painter->drawPixmap(QPointF(rect.x()+(rect.width()-logical.width())/2,rect.y()+(rect.height()-logical.height())/2),pix);
    }
};
QIcon icon(const ResultIdentity &id,const std::shared_ptr<Resources> &resources)
{
    if(!id.species)return {};
    const auto paths=resources->resolver.candidates(id);
    return paths.isEmpty()?QIcon():QIcon(new LazyIcon(resources,paths));
}
QStandardItemModel *sourceModel(QComboBox *combo)
{
    QAbstractItemModel *model=combo->model();
    for(int depth=0;depth<16;++depth)
    {
        auto *proxy=qobject_cast<QAbstractProxyModel *>(model);
        if(!proxy)break;
        model=proxy->sourceModel();
    }
    auto *standard=qobject_cast<QStandardItemModel *>(model);
    // Only the private standard item models created by the inspected controls.
    if(!standard || standard->parent()!=combo || typeid(*standard)!=typeid(QStandardItemModel))return nullptr;
    return standard;
}
class ChecklistDelegate final : public QStyledItemDelegate
{
    std::shared_ptr<Resources> resources;
    QPointer<QAbstractItemDelegate> original;
    Contract kind;
    QPointer<QComboBox> combo;
public:
    ChecklistDelegate(const std::shared_ptr<Resources> &r,QAbstractItemDelegate *delegate,Contract k,QComboBox *c)
        :resources(r),original(delegate),kind(k),combo(c){}
    void initStyleOption(QStyleOptionViewItem *option,const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option,index);
        if(!option->icon.isNull())return;
        option->icon=icon(identity(index,kind,*resources,combo),resources);
        if(!option->icon.isNull()) {option->features|=QStyleOptionViewItem::HasDecoration;option->decorationSize=spriteSize;}
    }
    bool editorEvent(QEvent *event,QAbstractItemModel *model,const QStyleOptionViewItem &option,const QModelIndex &index) override
    {
        return original ? original->editorEvent(event,model,option,index) : false;
    }
};
struct Binding final : QObject
{
    struct Entry {QPersistentModelIndex index;QVariant old;QIcon installed;ResultIdentity id;};
    QPointer<QComboBox> combo;
    QPointer<QStandardItemModel> model;
    Contract kind;
    std::shared_ptr<Resources> resources;
    std::function<void()> queue;
    std::vector<Entry> entries;
    QPointer<QAbstractItemView> popup;
    QPointer<QAbstractItemDelegate> originalDelegate;
    std::unique_ptr<ChecklistDelegate> delegate;
    QSize oldSize,oldPopupSize;
    bool dirty=true,editing=false,sized=false;
    Binding(QComboBox *c,Contract k,const std::shared_ptr<Resources> &r,std::function<void()> q)
        :combo(c),model(sourceModel(c)),kind(k),resources(r),queue(std::move(q)),oldSize(c->iconSize())
    {
        auto changed=[this]{if(!editing){dirty=true;queue();}};
        connect(model,&QAbstractItemModel::rowsInserted,this,changed);
        connect(model,&QAbstractItemModel::rowsRemoved,this,changed);
        connect(model,&QAbstractItemModel::modelReset,this,changed);
        connect(model,&QAbstractItemModel::layoutChanged,this,changed);
        connect(model,&QAbstractItemModel::dataChanged,this,changed);
        if(k==Contract::Template || k==Contract::Dream || k==Contract::Grotto || k==Contract::Raid || k==Contract::Form || k==Contract::RadarSlot)
            for(auto *context:owner(c)->findChildren<QComboBox *>())
                connect(context,&QComboBox::currentIndexChanged,this,changed);
        if(checklist(kind))
        {
            popup=c->view();oldPopupSize=popup->iconSize(); originalDelegate=popup->itemDelegate();
            delegate=std::make_unique<ChecklistDelegate>(r,originalDelegate,k,c);
            popup->setItemDelegate(delegate.get());popup->setIconSize(spriteSize);
        }
        refresh();
    }
    ~Binding() override
    {
        editing=true;
        for(const auto &entry:entries)restore(entry);
        if(combo && sized && combo->iconSize()==spriteSize)combo->setIconSize(oldSize);
        if(popup && popup->itemDelegate()==delegate.get())
        {
            popup->setItemDelegate(originalDelegate);
            if(popup->iconSize()==spriteSize)popup->setIconSize(oldPopupSize);
            popup->viewport()->update();
        }
    }
    void restore(const Entry &entry)
    {
        if(entry.index.isValid() && entry.index.data(Qt::DecorationRole).value<QIcon>().cacheKey()==entry.installed.cacheKey())
            const_cast<QAbstractItemModel *>(entry.index.model())->setData(entry.index,entry.old,Qt::DecorationRole);
    }
    void refresh()
    {
        if(!dirty || !combo || !model)return;
        dirty=false;
        if(checklist(kind)) {if(popup)popup->viewport()->update();return;}
        editing=true;
        // Persistent indices track real items through sorting, row insertion and removal.
        std::erase_if(entries,[this](const Entry &entry){
            if(!entry.index.isValid())return true;
            const auto now=identity(entry.index,kind,*resources,combo);
            if(now.species!=entry.id.species || now.form!=entry.id.form || now.gender!=entry.id.gender || now.shiny!=entry.id.shiny) {restore(entry);return true;}
            return entry.index.data(Qt::DecorationRole).value<QIcon>().cacheKey()!=entry.installed.cacheKey();
        });
        for(int row=0;row<model->rowCount();++row)
        {
            const auto index=model->index(row,combo->modelColumn());
            if(std::any_of(entries.begin(),entries.end(),[&](const Entry &e){return e.index==index;}))continue;
            const auto old=index.data(Qt::DecorationRole);
            if(old.isValid() && (!old.canConvert<QIcon>() || !old.value<QIcon>().isNull()))continue;
            const auto id=identity(index,kind,*resources,combo);const auto decoration=icon(id,resources);
            if(decoration.isNull())continue;
            entries.push_back({index,old,decoration,id});
            model->setData(index,decoration,Qt::DecorationRole);
        }
        if(!entries.empty() && !sized) {combo->setIconSize(spriteSize);sized=true;}
        if(entries.empty() && sized) {if(combo->iconSize()==spriteSize)combo->setIconSize(oldSize);sized=false;}
        editing=false;
    }
    bool valid() const
    {
        return combo && model && sourceModel(combo)==model && contract(combo)==kind
            && (!delegate || (popup==combo->view() && popup->itemDelegate()==delegate.get()));
    }
};
}
struct PokemonControls::Impl
{
    std::shared_ptr<Resources> resources;
    std::vector<std::unique_ptr<Binding>> bindings;
    bool queued=false,scanning=false;
    Impl(SpriteResolver &r,SpriteCache &c):resources(std::make_shared<Resources>(r,c)){}
};
PokemonControls::PokemonControls(SpriteResolver &r,SpriteCache &c):impl(std::make_unique<Impl>(r,c))
{
    scan();qApp->installEventFilter(this);
}
PokemonControls::~PokemonControls()
{
    qApp->removeEventFilter(this);impl->bindings.clear();impl->resources.reset();
}
void PokemonControls::queue()
{
    if(impl->queued || impl->scanning)return;
    impl->queued=true;QTimer::singleShot(0,this,[this]{impl->queued=false;scan();});
}
bool PokemonControls::eventFilter(QObject *object,QEvent *event)
{
    if(event->type()==QEvent::Show || event->type()==QEvent::ChildPolished || event->type()==QEvent::Destroy)queue();
    if(event->type()==QEvent::Paint)
        if(auto *combo=qobject_cast<QComboBox *>(object))
            for(const auto &binding:impl->bindings)if(binding->combo==combo && !binding->valid()){queue();break;}
    return false;
}
void PokemonControls::scan()
{
    if(impl->scanning)return;
    impl->scanning=true;
    std::erase_if(impl->bindings,[](const auto &b){return !b->valid();});
    for(const auto &b:impl->bindings)b->refresh();
    for(auto *widget:QApplication::allWidgets())
    {
        auto *combo=qobject_cast<QComboBox *>(widget);
        if(!combo || std::any_of(impl->bindings.begin(),impl->bindings.end(),[combo](const auto &b){return b->combo==combo;}))continue;
        const auto kind=contract(combo);
        if(kind==Contract::None || !sourceModel(combo))continue;
        if(checklist(kind))
        {
            const auto *delegate=combo->view()->itemDelegate();
            const QByteArray type=delegate?QByteArray(typeid(*delegate).name()):QByteArray();
            if(type!="class QComboBoxDelegate" && type!="class QComboMenuDelegate" && type!="class QStyledItemDelegate")continue;
        }
        impl->bindings.push_back(std::make_unique<Binding>(combo,kind,impl->resources,[this]{queue();}));
    }
    impl->scanning=false;
}
