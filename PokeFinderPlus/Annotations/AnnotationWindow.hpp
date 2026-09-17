#ifndef POKEFINDERPLUS_ANNOTATIONWINDOW_HPP
#define POKEFINDERPLUS_ANNOTATIONWINDOW_HPP

#include <QDialog>
#include <QColor>
#include <array>

class AnnotationController;
class QLabel;
class QSlider;

class AnnotationWindow : public QDialog
{
public:
    AnnotationWindow();
protected:
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    struct ColorSlot { QColor color; int opacity = 55; };
    void selectColor(int slot);
    void updateColor();
    std::array<ColorSlot, 4> colors {{ { QColor("#F4A6A6") }, { QColor("#A8D5BA") },
                                      { QColor("#F6E7A1") }, { QColor("#A9C7E8") } }};
    int selectedColor = 0;
    QSlider *opacity;
    QLabel *percentage;
    AnnotationController *controller;
};

#endif
