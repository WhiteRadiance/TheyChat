#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <QTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QTabWidget>


class subwin_text : public QWidget
{
    Q_OBJECT

signals:
public slots:
    void lineEdit_limit();
public:
    QTextEdit   *textEdit;
    QLineEdit   *lineEdit;
    QWidget     *EmpWid;
    QString      id;

    QSize sizeHint() const { return QSize(500, 370); }//默认大小(w,h)
    explicit subwin_text(QWidget *parent=nullptr, bool empty=false);
    ~subwin_text()
    {
        if(!textEdit)   delete textEdit;
        if(!lineEdit)   delete lineEdit;
        if(!EmpWid)     delete EmpWid;
    }
};




class subwin_list : public QWidget
{
    Q_OBJECT

signals:
public slots:
public:
    QListWidget *listWidget;
    QSize sizeHint() const { return QSize(180, 370); }//默认大小(w,h)
    explicit subwin_list(QWidget *parent=nullptr);
};




class TabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit TabWidget(QWidget *parent=nullptr) : QTabWidget(parent) {}
    QTabBar *tabbar() { return this->tabBar(); }
};


#endif // DOCKWINDOW_H
