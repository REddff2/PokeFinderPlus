#pragma once
#include <QObject>
#include <memory>
class SpriteResolver;
class SpriteCache;
class PokemonControls final : public QObject
{
public:
    PokemonControls(SpriteResolver &resolver,SpriteCache &cache);
    ~PokemonControls() override;
    void scan();
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    void queue();
};
