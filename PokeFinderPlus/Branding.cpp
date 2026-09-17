#include "Branding.hpp"

#include <QApplication>
#include <QEvent>
#include <QIcon>
#include <QWidget>

namespace
{
class WindowBranding final : public QObject
{
public:
    explicit WindowBranding(QApplication &application) : QObject(&application) {}

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::Show)
        {
            auto *window = qobject_cast<QWidget *>(object);
            if (window && window->isWindow() && window->windowType() != Qt::Popup && window->windowType() != Qt::ToolTip)
            {
                // Some inherited .ui files set an explicit original icon. Apply
                // the fork's branding after setupUi, without editing those forms.
                window->setWindowIcon(QApplication::windowIcon());
            }
        }
        return false;
    }
};
}

void PokeFinderPlus::applyBranding(QApplication &application)
{
    application.setWindowIcon(QIcon(QStringLiteral(":/PokeFinderPlus/pokefinder-plus.ico")));
    application.installEventFilter(new WindowBranding(application));
}
