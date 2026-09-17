// Standalone Annotations verification host; never shipped in either package.
#include "../PluginApi.hpp"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLibrary>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
    std::cout << "PASS: " << message << std::endl;
}

template <typename T> T *button(QWidget *window, const QString &text)
{
    for (auto *item : window->findChildren<T *>())
        if (item->text() == text)
            return item;
    throw std::runtime_error(("Missing button: " + text).toStdString());
}

QDialog *dialog(QWidget *host)
{
    const auto dialogs = host->findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly);
    check(dialogs.size() == 1, "exactly one plugin window");
    return dialogs.first();
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2)
        return 1;
    QLibrary library(QString::fromLocal8Bit(argv[1]));
    if (!library.load())
    {
        std::cerr << library.errorString().toStdString() << std::endl;
        return 2;
    }
    auto version = reinterpret_cast<PokeFinderPlusPluginApiVersion>(library.resolve("pokefinder_plus_plugin_api_version"));
    auto name = reinterpret_cast<PokeFinderPlusPluginName>(library.resolve("pokefinder_plus_plugin_name"));
    auto initialize = reinterpret_cast<PokeFinderPlusPluginInitialize>(library.resolve("pokefinder_plus_plugin_initialize"));
    auto open = reinterpret_cast<PokeFinderPlusPluginOpenWindow>(library.resolve("pokefinder_plus_plugin_open_window"));
    auto shutdown = reinterpret_cast<PokeFinderPlusPluginShutdown>(library.resolve("pokefinder_plus_plugin_shutdown"));
    if (!version || !name || !initialize || !open || !shutdown)
        return 3;
    QWidget host;
    host.setWindowTitle("Annotations verification host");
    host.show();
    try
    {
        check(version() == POKEFINDER_PLUS_PLUGIN_API_VERSION && QString::fromUtf8(name()) == "Annotations", "plugin identity and API");
        check(initialize(&host), "initialize");
        open(&host);
        app.processEvents();
        QPointer<QDialog> window = dialog(&host);
        check(window->isVisible() && window->windowTitle() == "Annotations" && !window->isModal(), "visible native modeless Annotations window");
        auto *cell = button<QRadioButton>(window, "Cell");
        auto *row = button<QRadioButton>(window, "Row");
        check(cell->isChecked() && !row->isChecked(), "default Cell mode");
        for (auto *selected : { row, cell, row })
        {
            selected->click();
            check(selected->isChecked() && cell->isChecked() != row->isChecked(), "exclusive mode selection");
        }
        QList<QPushButton *> colors;
        for (const auto &text : { "Red", "Green", "Yellow", "Blue" })
            colors.append(button<QPushButton>(window, text));
        check(colors.first()->isChecked(), "default Red color");
        for (auto *selected : colors)
        {
            selected->click();
            selected->click();
            int checked = 0;
            for (auto *color : colors)
                checked += color->isChecked();
            check(selected->isChecked() && checked == 1, "exactly one selected color, including repeat clicks");
        }
        auto *status = window->findChild<QLabel *>("annotationStatus");
        check(status && status->text() == "Ready", "initial Ready status");
        const QStringList actions = { "Mark", "Clear", "Clear All" };
        const QStringList messages = { "Mark mode armed", "Clear mode armed", "No annotations to clear" };
        for (int i = 0; i < actions.size(); ++i)
        {
            auto *action = button<QPushButton>(window, actions[i]);
            check(!action->isCheckable(), "normal action button");
            action->click();
            app.processEvents();
            check(status->text() == messages[i], "exact action status text");
            check(row->isChecked() && colors.last()->isChecked(), "actions preserve mode and color");
            for (auto *widget : QApplication::topLevelWidgets())
                check(!widget->isVisible() || widget == &host || widget == window.data(), "no extra popup");
        }
        for (int i = 0; i < 3; ++i)
        {
            window->close();
            app.processEvents();
            check(window && !window->isVisible(), "close hides window");
            open(&host);
            app.processEvents();
            check(dialog(&host) == window.data() && window->isVisible(), "reopen reuses window");
            check(row->isChecked() && colors.last()->isChecked() && status->text() == messages.last(), "reopen retains UI state");
        }
        open(&host);
        check(dialog(&host) == window.data(), "Open while visible reuses window");
        shutdown();
        check(window.isNull(), "shutdown destroys visible window");
        shutdown();
        check(initialize(&host), "reinitialize after shutdown");
        open(&host);
        window = dialog(&host);
        window->close();
        shutdown();
        check(window.isNull(), "shutdown destroys hidden window");
        auto *temporaryHost = new QWidget;
        check(initialize(temporaryHost), "initialize with replacement host");
        open(temporaryHost);
        window = dialog(temporaryHost);
        delete temporaryHost;
        check(window.isNull(), "host destruction deletes child window");
        shutdown();
        check(initialize(&host), "reinitialize after host destruction");
        open(&host);
        window = dialog(&host);
        check(button<QRadioButton>(window, "Cell")->isChecked() && button<QPushButton>(window, "Red")->isChecked(), "fresh lifecycle restores defaults");
        shutdown();
        std::cout << "ALL ANNOTATIONS UI/LIFECYCLE CHECKS PASSED" << std::endl;
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << std::endl;
        shutdown();
        return 4;
    }
}
