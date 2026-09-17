#include "SpriteResolver.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>

SpriteResolver::SpriteResolver(const QString &directory) : directory(directory)
{
    if (directory.isEmpty()) return;
    QFile file(QDir(directory).filePath("manifest.json"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) return;
    const auto data = QJsonDocument::fromJson(file.readAll()).object();
    if (data.value("schema").toInt() != 1) return;
    pokemon = data.value("pokemon").toObject();
    items = data.value("items").toObject();
}
QStringList SpriteResolver::candidates(const ResultIdentity &identity) const
{
    QStringList result;
    const auto add = [&](const QString &path) {
        if (path.isEmpty() || path.contains("..") || path.contains('\\') || path.contains(':')
            || !(path.startsWith("pokemon-gen8/") || path.startsWith("items/")) || !path.endsWith(".png")) return;
        const auto full = QDir(directory).filePath("sprites/" + path);
        if (!result.contains(full)) result.append(full);
    };
    if (identity.item > 0) { add(items.value(QString::number(identity.item)).toString()); return result; }
    const auto forms = pokemon.value(QString::number(identity.species)).toObject();
    const auto form = forms.value(QString::number(identity.form < 0 ? 0 : identity.form)).toObject();
    const QString variant = identity.shiny ? "shiny" : "normal";
    if (identity.gender == 1) add(form.value(variant + "Female").toString());
    add(form.value(variant).toString());
    add(form.value("normal").toString());
    add(forms.value("0").toObject().value("normal").toString());
    return result;
}
