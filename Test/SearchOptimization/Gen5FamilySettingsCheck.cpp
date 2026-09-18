#include <Form/Util/Settings.hpp>
#include <Form/Util/SearchOptimizationSettings.hpp>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QGroupBox>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <iostream>
#include <stdexcept>

using Family = SearchOptimization::Family;
static constexpr std::array<const char *, 9> Names = {
    "Wild", "Static", "Event", "Eggs", "DreamRadar", "HiddenGrotto", "Pickup", "AdjacentSeeds", "CacheBuilders"
};
static constexpr std::array<const char *, 9> Labels = {
    "Wild", "Static", "Event", "Eggs", "Dream Radar", "Hidden Grotto", "Pickup", "Adjacent Seeds", "Cache Builders"
};
static void check(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("Gen5FamilySettingsTest");
    app.setApplicationName("Gen5FamilySettingsTest");
    app.setQuitOnLastWindowClosed(false);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    app.setFont(QFont("Segoe UI", 9));
    app.setStyle("fusion");
    try
    {
        check(argc >= 2, "output directory required");
        QString output = QString::fromLocal8Bit(argv[1]);
        if (argc == 2)
        {
            QTemporaryDir store(QDir(output).filePath("FamilySettings-XXXXXX"));
            check(store.isValid(), "isolated settings store");
            for (const char *step : {"clean", "read-clean", "on", "read-on", "gpu", "read-gpu", "wild-off", "read-wild",
                                    "static-off", "read-static", "mixed", "read-mixed", "off", "read-off", "restore", "read-restored",
                                    "legacy-cpu", "legacy-gpu", "invalid"})
            {
                QProcess child;
                child.start(app.applicationFilePath(), {output, store.path(), step});
                check(child.waitForStarted(5000) && child.waitForFinished(20000), "settings restart completed");
                std::cout << child.readAllStandardOutput().toStdString();
                std::cerr << child.readAllStandardError().toStdString();
                check(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, "settings restart case passed");
            }
            std::cout << "FAMILY_SETTINGS_RESTART_PASS 19 separate processes\n";
            return 0;
        }
        check(argc == 4, "child arguments");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, argv[2]);
        QSettings saved;
        QString step = argv[3];
        if (step == "clean" || step.startsWith("legacy"))
        {
            saved.clear();
            saved.setValue("settings/locale", "en");
            saved.setValue("settings/threads", QThread::idealThreadCount());
            if (step.startsWith("legacy"))
            {
                saved.setValue("settings/plusSearchPruning", true);
                saved.setValue("settings/plusSearchGpu", step == "legacy-gpu");
            }
        }
        if (step == "invalid")
        {
            saved.setValue("settings/plusSearchPruning", false);
            saved.setValue("settings/plusSearchGpu", true);
        }
        // Exercise the exact startup loader before opening Settings.
        saved.beginGroup("settings");
        SearchOptimizationSettings::load(saved);
        saved.endGroup();
        check(SearchOptimization::pruningEnabled() == saved.value("settings/plusSearchPruning", false).toBool(), "startup master");
        for (size_t i = 0; i < Names.size(); ++i)
            check(SearchOptimization::familyEnabled(static_cast<Family>(i))
                == saved.value(QString("settings/") + SearchOptimization::FamilyKeys[i], true).toBool(), "startup family preference");

        Settings settings;
        settings.setAttribute(Qt::WA_DeleteOnClose, false);
        settings.show();
        QTest::qWait(60);
        auto *master = settings.findChild<QCheckBox *>("checkBoxSearchPruning");
        auto *gpu = settings.findChild<QCheckBox *>("checkBoxSearchGpu");
        auto *group = settings.findChild<QGroupBox *>("groupBoxSearchOptimization");
        auto *children = settings.findChild<QWidget *>("widgetSearchFamilies");
        auto *threads = settings.findChild<QComboBox *>("comboBoxThreads");
        check(master && gpu && group && children && threads, "native controls present");
        check(group->findChildren<QCheckBox *>().size() == 11, "master, nine children and separate GPU");
        check(threads->count() == QThread::idealThreadCount(), "normal Threads unchanged");
        std::array<QCheckBox *, 9> boxes;
        for (size_t i = 0; i < Names.size(); ++i)
        {
            boxes[i] = settings.findChild<QCheckBox *>(QString("checkBoxSearch") + Names[i]);
            check(boxes[i] && boxes[i]->text() == Labels[i] && boxes[i]->parentWidget() == children, "family labels and hierarchy");
        }
        if (step == "on" || step == "restore") master->setChecked(true);
        if (step == "gpu") gpu->setChecked(true);
        if (step == "wild-off") boxes[0]->setChecked(false);
        if (step == "static-off") boxes[1]->setChecked(false);
        if (step == "mixed") for (size_t i = 0; i < boxes.size(); ++i) boxes[i]->setChecked(i >= 2 && i % 2 == 0);
        if (step == "off") master->setChecked(false);

        const bool wantMaster = step != "clean" && step != "read-clean" && step != "off" && step != "read-off" && step != "invalid";
        const bool mixed = step == "mixed" || step == "read-mixed" || step == "off" || step == "read-off"
            || step == "restore" || step == "read-restored";
        const bool wildOff = mixed || step == "wild-off" || step == "read-wild" || step == "static-off" || step == "read-static";
        const bool staticOff = mixed || step == "static-off" || step == "read-static";
        const bool wantGpu = step == "gpu" || step == "read-gpu" || step == "wild-off" || step == "read-wild"
            || step == "static-off" || step == "read-static" || step == "mixed" || step == "read-mixed" || step == "legacy-gpu";
        check(master->isChecked() == wantMaster && gpu->isChecked() == wantGpu && gpu->isEnabled() == wantMaster, "master/GPU dependency");
        check(SearchOptimization::pruningEnabled() == wantMaster && SearchOptimization::gpuEnabled() == wantGpu, "runtime master/GPU");
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            bool want = mixed ? i >= 2 && i % 2 == 0 : !(i == 0 && wildOff) && !(i == 1 && staticOff);
            auto family = static_cast<Family>(i);
            check(boxes[i]->isChecked() == want && boxes[i]->isEnabled() == wantMaster, "child selection/enabled state");
            check(SearchOptimization::familyEnabled(family) == want, "raw child preference survives master");
            check(SearchOptimization::pruningEnabled(family) == (wantMaster && want), "effective CPU family policy");
            check(SearchOptimization::gpuEnabled(family) == (wantGpu && want), "effective GPU family policy");
            check(saved.value(QString("settings/") + SearchOptimization::FamilyKeys[i]).toBool() == want, "saved child preference");
        }
        if (step == "clean" || step == "on" || step == "mixed" || step == "off" || step == "restore")
            settings.grab().save(QDir(output).filePath("families-" + step + ".png"));
        saved.sync();
        settings.close();
        std::cout << "FAMILY_SETTINGS_PASS " << step.toStdString() << '\n';
        return 0;
    }
    catch (const std::exception &e) { std::cerr << "FAMILY_SETTINGS_FAIL " << e.what() << '\n'; return 1; }
}
