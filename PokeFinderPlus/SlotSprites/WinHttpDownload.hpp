#pragma once
#include <QString>
#include <atomic>

namespace SlotSprites
{
void downloadArchive(const QString &url, const QString &destination, std::atomic_bool &cancel,
                     std::atomic_int &progress);
}
