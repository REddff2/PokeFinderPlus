#pragma once
#include "SpriteCache.hpp"
#include "SpriteResolver.hpp"
#include <QObject>
#include <memory>
#include <vector>
class PokemonControls;

class SlotSpritesController final : public QObject
{
public:
    SlotSpritesController();
    ~SlotSpritesController() override;
    bool ready() const { return resolver.ready(); }
    void scan();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    struct Binding;
    void queueScan();
    SpriteResolver resolver;
    SpriteCache cache;
    std::unique_ptr<PokemonControls> controls;
    std::vector<std::unique_ptr<Binding>> bindings;
    bool queued = false;
    bool scanning = false;
};
