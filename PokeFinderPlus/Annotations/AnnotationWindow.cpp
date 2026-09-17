#include "AnnotationWindow.hpp"
#include "AnnotationController.hpp"

#include <QButtonGroup>
#include <QAbstractButton>
#include <QColorDialog>
#include <QPainter>
#include <QSlider>
#include <QSignalBlocker>
#include <QStyleOptionFocusRect>
#include <QGroupBox>
#include <QHideEvent>
#include <QShowEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace
{
class ColorPreview final : public QAbstractButton
{
public:
    explicit ColorPreview(const QColor &color, QWidget *parent) : QAbstractButton(parent), color(color)
    {
        setFixedSize(20, 20);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
    }
    void setColor(const QColor &value) { color = value; update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(palette().color(QPalette::Mid));
        painter.setBrush(color);
        painter.drawEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        if (hasFocus())
        {
            QStyleOptionFocusRect option;
            option.initFrom(this);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
        }
    }
private:
    QColor color;
};
}

AnnotationWindow::AnnotationWindow() : QDialog(nullptr, Qt::Window), controller(new AnnotationController(this))
{
    setWindowTitle(tr("Annotations"));
    setWindowModality(Qt::NonModal);
    // The plugin owns this independent window and closes it on shutdown.
    // It must not keep the application alive after the main window closes.
    setAttribute(Qt::WA_QuitOnClose, false);
    // Closing hides the window so selections survive the next Open request.
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);
    auto *modeBox = new QGroupBox(tr("Mode"), this);
    auto *modeLayout = new QVBoxLayout(modeBox);
    auto *modeGroup = new QButtonGroup(modeBox);
    modeGroup->setExclusive(true);
    auto *cell = new QRadioButton(tr("Cell"), modeBox);
    auto *row = new QRadioButton(tr("Row"), modeBox);
    modeGroup->addButton(cell);
    modeGroup->addButton(row);
    modeLayout->addWidget(cell);
    modeLayout->addWidget(row);
    cell->setChecked(true);
    connect(row, &QRadioButton::toggled, controller, &AnnotationController::setRowMode);
    layout->addWidget(modeBox);

    auto *colorBox = new QGroupBox(tr("Color"), this);
    auto *colorLayout = new QVBoxLayout(colorBox);
    auto *colorGroup = new QButtonGroup(colorBox);
    colorGroup->setExclusive(true);
    for (int i = 0; i < int(colors.size()); ++i)
    {
        auto *line = new QHBoxLayout;
        auto *preview = new ColorPreview(colors[i].color, colorBox);
        preview->setObjectName(QStringLiteral("annotationColorPreview%1").arg(i + 1));
        preview->setAccessibleName(tr("Choose Color %1").arg(i + 1));
        preview->setToolTip(tr("Choose Color %1").arg(i + 1));
        auto *button = new QPushButton(tr("Color %1").arg(i + 1), colorBox);
        button->setObjectName(QStringLiteral("annotationColorSlot%1").arg(i + 1));
        button->setAutoDefault(false);
        button->setCheckable(true);
        connect(button, &QPushButton::clicked, this, [this, i] { selectColor(i); });
        connect(preview, &QAbstractButton::clicked, this, [this, i, preview] {
            const QColor chosen = QColorDialog::getColor(colors[i].color, this, tr("Choose Color %1").arg(i + 1));
            if (!chosen.isValid()) return;
            colors[i].color = chosen;
            preview->setColor(chosen);
            if (selectedColor == i) updateColor();
        });
        colorGroup->addButton(button);
        line->addWidget(preview);
        line->addWidget(button);
        colorLayout->addLayout(line);
        button->setChecked(i == 0);
    }
    layout->addWidget(colorBox);

    auto *opacityBox = new QGroupBox(tr("Transparency"), this);
    auto *opacityLayout = new QHBoxLayout(opacityBox);
    opacity = new QSlider(Qt::Horizontal, opacityBox);
    opacity->setObjectName(QStringLiteral("annotationOpacity"));
    opacity->setAccessibleName(tr("Transparency"));
    opacity->setToolTip(tr("Annotation strength: 0% is invisible; 100% is the full color."));
    opacity->setRange(0, 100);
    percentage = new QLabel(opacityBox);
    percentage->setObjectName(QStringLiteral("annotationOpacityPercentage"));
    percentage->setMinimumWidth(percentage->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
    opacityLayout->addWidget(opacity);
    opacityLayout->addWidget(percentage);
    layout->addWidget(opacityBox);
    connect(opacity, &QSlider::valueChanged, this, [this](int value) {
        colors[selectedColor].opacity = value;
        updateColor();
    });
    selectColor(0);

    auto *actionBox = new QGroupBox(tr("Action"), this);
    auto *actionLayout = new QHBoxLayout(actionBox);
    auto *mark = new QPushButton(tr("Mark"), actionBox);
    auto *clear = new QPushButton(tr("Clear"), actionBox);
    mark->setAutoDefault(false);
    clear->setAutoDefault(false);
    actionLayout->addWidget(mark);
    actionLayout->addWidget(clear);
    layout->addWidget(actionBox);

    auto *clearAll = new QPushButton(tr("Clear All"), this);
    clearAll->setAutoDefault(false);
    layout->addWidget(clearAll);
    layout->addWidget(new QLabel(tr("Status:"), this));
    auto *status = new QLabel(tr("Ready"), this);
    status->setObjectName(QStringLiteral("annotationStatus"));
    layout->addWidget(status);

    connect(mark, &QPushButton::clicked, controller, &AnnotationController::mark);
    connect(clear, &QPushButton::clicked, controller, &AnnotationController::clear);
    connect(clearAll, &QPushButton::clicked, controller, &AnnotationController::clearAll);
    connect(controller, &AnnotationController::statusChanged, status, &QLabel::setText);
    resize(240, sizeHint().height());
}

void AnnotationWindow::selectColor(int slot)
{
    selectedColor = slot;
    const QSignalBlocker blocker(opacity);
    opacity->setValue(colors[slot].opacity);
    updateColor();
}

void AnnotationWindow::updateColor()
{
    percentage->setText(tr("%1%").arg(colors[selectedColor].opacity));
    QColor tint = colors[selectedColor].color;
    tint.setAlphaF(colors[selectedColor].opacity / 100.0);
    // The controller stores QColor by value, including alpha. Later slot edits
    // therefore affect only future marks, not existing persistent identities.
    controller->setColor(tint);
}

void AnnotationWindow::hideEvent(QHideEvent *event)
{
    if (!event->spontaneous())
        controller->stop();
    QDialog::hideEvent(event);
}

void AnnotationWindow::showEvent(QShowEvent *event)
{
    controller->start();
    QDialog::showEvent(event);
}
