#pragma once
class QWidget;
namespace SlotSprites
{
// Returns success only after all required local files have been validated.
// Networking is asynchronous; validation/extraction run on a worker thread.
bool ensureAssets(QWidget *host);
void cancelAssetSetup();
}
