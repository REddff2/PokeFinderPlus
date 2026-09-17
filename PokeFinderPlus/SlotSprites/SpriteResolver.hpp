#pragma once
#include "ResultIdentity.hpp"
#include <QJsonObject>
#include <QStringList>

class SpriteResolver
{
public:
    explicit SpriteResolver(const QString &directory);
    bool ready() const { return !pokemon.isEmpty(); }
    QStringList candidates(const ResultIdentity &identity) const;
private:
    QString directory;
    QJsonObject pokemon, items;
};
