/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Settings.hpp"
#include "ui_Settings.h"
#include <Core/Parents/ProfileLoader.hpp>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QThread>
#include "SearchOptimizationSettings.hpp"

Settings::Settings(QWidget *parent) : QWidget(parent), ui(new Ui::Settings)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->widgetSearchFamilies->hide();
    connect(ui->toolButtonSearchAdvanced, &QToolButton::toggled, this, [this](bool expanded) {
        ui->toolButtonSearchAdvanced->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        ui->widgetSearchFamilies->setVisible(expanded);
        ui->verticalLayoutSearchOptimization->activate();
        layout()->activate();
        resize(width(), sizeHint().height());
    });

    QSettings setting;
    setting.beginGroup("settings");

    // Language
    QString language = setting.value("locale").toString();
    QStringList languages = { "zh", "en", "fr", "de", "it", "ja", "ko", "es" };
    for (int i = 0; i < languages.size(); i++)
    {
        const QString &lang = languages[i];
        ui->comboBoxLanguage->setItemData(i, lang);
        if (language == lang)
        {
            ui->comboBoxLanguage->setCurrentIndex(i);
        }
    }

    // Profiles
    QString profile = setting.value("profiles").toString();
    ui->lineEditProfiles->setText(profile);

    // Style
    QString style = setting.value("style").toString();
    QStringList styles = { "auto", "dark", "light" };
    for (int i = 0; i < styles.size(); i++)
    {
        const QString &sty = styles[i];
        ui->comboBoxStyle->setItemData(i, sty);
        if (style == sty)
        {
            ui->comboBoxStyle->setCurrentIndex(i);
        }
    }

    // Table header size
    auto size = setting.value("headerSize").value<QHeaderView::ResizeMode>();
    std::array<QHeaderView::ResizeMode, 2> sizes = { QHeaderView::ResizeToContents, QHeaderView::Stretch };
    for (int i = 0; i < sizes.size(); i++)
    {
        auto s = sizes[i];
        ui->comboBoxTableHeaderSize->setItemData(i, s);
        if (size == s)
        {
            ui->comboBoxTableHeaderSize->setCurrentIndex(i);
        }
    }

    // Threads
    int threads = setting.value("threads").toInt();
    for (int i = 1; i <= QThread::idealThreadCount(); i++)
    {
        ui->comboBoxThreads->addItem(QString::number(i), i);
        if (i == threads)
        {
            ui->comboBoxThreads->setCurrentIndex(i - 1);
        }
    }

    SearchOptimizationSettings::load(setting);
    const bool smart = SearchOptimization::pruningEnabled();
    const bool gpu = SearchOptimization::gpuEnabled();
    ui->checkBoxSearchPruning->setChecked(smart);
    ui->checkBoxSearchGpu->setChecked(gpu);
    ui->checkBoxSearchGpu->setEnabled(smart);
    ui->widgetSearchFamilies->setEnabled(smart);
    ui->toolButtonSearchAdvanced->setVisible(smart);
    const std::array<QCheckBox *, SearchOptimization::FamilyCount> familyBoxes = {
        ui->checkBoxSearchWild,
        ui->checkBoxSearchStatic,
        ui->checkBoxSearchEvent,
        ui->checkBoxSearchEggs,
        ui->checkBoxSearchDreamRadar,
        ui->checkBoxSearchHiddenGrotto,
        ui->checkBoxSearchPickup,
        ui->checkBoxSearchAdjacentSeeds,
        ui->checkBoxSearchCacheBuilders
    };
    for (size_t i = 0; i < familyBoxes.size(); ++i)
    {
        const auto family = static_cast<SearchOptimization::Family>(i);
        const QString key = QStringLiteral("settings/") + SearchOptimization::FamilyKeys[i];
        auto *box = familyBoxes[i];
        box->setChecked(SearchOptimization::familyEnabled(family));
        connect(box, &QCheckBox::toggled, this, [family, key](bool enabled) {
            SearchOptimization::setFamilyEnabled(family, enabled);
            QSettings().setValue(key, enabled);
        });
    }
    setting.endGroup();

    connect(ui->checkBoxSearchPruning, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings settings;
        settings.setValue("settings/plusSearchPruning", enabled);
        SearchOptimization::setPruningEnabled(enabled);
        if (!enabled)
        {
            ui->checkBoxSearchGpu->setChecked(false);
            settings.setValue("settings/plusSearchGpu", false);
        }
        ui->checkBoxSearchGpu->setEnabled(enabled);
        ui->widgetSearchFamilies->setEnabled(enabled);
        ui->toolButtonSearchAdvanced->setVisible(enabled);
        if (!enabled)
        {
            ui->toolButtonSearchAdvanced->setChecked(false);
        }
        ui->verticalLayoutSearchOptimization->activate();
        layout()->activate();
        resize(width(), sizeHint().height());
    });
    connect(ui->checkBoxSearchGpu, &QCheckBox::toggled, this, [this](bool enabled) {
        enabled = enabled && ui->checkBoxSearchPruning->isChecked();
        SearchOptimization::setGpuEnabled(enabled);
        QSettings().setValue("settings/plusSearchGpu", enabled);
    });

    connect(ui->comboBoxLanguage, &QComboBox::currentIndexChanged, this, &Settings::languageIndexChanged);
    connect(ui->pushButtonProfile, &QPushButton::clicked, this, &Settings::changeProfiles);
    connect(ui->comboBoxStyle, &QComboBox::currentIndexChanged, this, &Settings::styleIndexChanged);
    connect(ui->comboBoxTableHeaderSize, &QComboBox::currentIndexChanged, this, &Settings::tableHeaderSizeIndexChanged);
    connect(ui->comboBoxThreads, &QComboBox::currentIndexChanged, this, &Settings::threadsIndexChanged);

    if (setting.contains("settingsForm/geometry"))
    {
        this->restoreGeometry(setting.value("settingsForm/geometry").toByteArray());
    }
    layout()->activate();
    resize(width(), sizeHint().height());
}

Settings::~Settings()
{
    QSettings setting;
    setting.setValue("settingsForm/geometry", this->saveGeometry());

    delete ui;
}

void Settings::changeProfiles()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Select Profile json", QDir::currentPath(), "json (*.json)");
    if (!fileName.isEmpty())
    {
        if (!QFile::exists(fileName))
        {
            QFile f(fileName);
            if (!f.open(QIODevice::WriteOnly))
            {
                QMessageBox msg(QMessageBox::Information, tr("Profile File"), tr("There was a problem creating the file"));
                msg.exec();
                return;
            }
        }

        QSettings setting;
        setting.setValue("settings/profiles", fileName);

        ProfileLoader::init(fileName.toStdWString());

        ui->lineEditProfiles->setText(fileName);
    }
}

void Settings::languageIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        QString currentLanguage = setting.value("settings/locale").toString();
        QString language = ui->comboBoxLanguage->currentData().toString();

        if (currentLanguage != language)
        {
            setting.setValue("settings/locale", language);

            QMessageBox msg(QMessageBox::Question, tr("Language update"), tr("Restart for changes to take effect. Restart now?"),
                            QMessageBox::Yes | QMessageBox::No);
            if (msg.exec() == QMessageBox::Yes)
            {
                QProcess::startDetached(QApplication::applicationFilePath());
                QApplication::quit();
            }
        }
    }
}

void Settings::styleIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        QString currentStyle = setting.value("settings/style").toString();
        QString style = ui->comboBoxStyle->currentData().toString();
        if (currentStyle != style)
        {
            setting.setValue("settings/style", style);

            QMessageBox msg(QMessageBox::Question, tr("Style change"), tr("Restart for changes to take effect. Restart now?"),
                            QMessageBox::Yes | QMessageBox::No);
            if (msg.exec() == QMessageBox::Yes)
            {
                QProcess::startDetached(QApplication::applicationFilePath());
                QApplication::quit();
            }
        }
    }
}

void Settings::tableHeaderSizeIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        setting.setValue("settings/headerSize", ui->comboBoxTableHeaderSize->currentData());
    }
}

void Settings::threadsIndexChanged(int index)
{
    if (index >= 0)
    {
        QSettings setting;
        setting.setValue("settings/threads", ui->comboBoxThreads->currentData().toInt());
    }
}
