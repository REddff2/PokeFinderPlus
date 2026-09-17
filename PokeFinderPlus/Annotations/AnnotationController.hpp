#ifndef POKEFINDERPLUS_ANNOTATIONCONTROLLER_HPP
#define POKEFINDERPLUS_ANNOTATIONCONTROLLER_HPP

#include <QObject>
#include <QColor>
#include <QPointer>
#include <memory>
#include <vector>

class QTableView;
class QWidget;

class AnnotationController final : public QObject
{
    Q_OBJECT
public:
    explicit AnnotationController(QWidget *window);
    ~AnnotationController() override;
    void mark();
    void clear();
    void clearAll();
    void setRowMode(bool enabled);
    void setColor(const QColor &color);
    void start();
    void stop();

signals:
    void statusChanged(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct TableBinding;
    void apply(bool marking);
    QTableView *tableForWidget(QWidget *widget) const;
    QTableView *targetTable();
    void remember(QWidget *widget);
    TableBinding *bindingFor(QTableView *view, bool create);
    QWidget *window;
    bool rowMode = false;
    QColor color = Qt::red;
    bool tracking = false;
    QMetaObject::Connection focusConnection;
    QPointer<QTableView> lastTable;
    std::vector<std::unique_ptr<TableBinding>> tables;
};
#endif
