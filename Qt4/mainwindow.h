#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logindialog.h"
#include "dockwindow.h"
#include <QTcpSocket>
#include <QDockWidget>

#include <QDockWidget>

#include <map>
using namespace std;


#define BUFFER_SIZE     4096


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    LoginDialog             *pLogDia;
    QTcpSocket              *sock;      // connect to server
    QString                  Qstr_idnum;
    char                    *ibuf;
    QString                  Qstr_response;


signals:

private:
    QDockWidget             *Dock0;     // List
    QDockWidget             *Dock1;     // Tabs
    //map<QString, dockwin*>   mDock;

    subwin_list             *list;
    TabWidget               *tabWidget; // not QTabWidget
    subwin_text             *tab0;
    //QWidget                 *empty;

    map<QString, subwin_text*>  mTab;


public slots:
    void When_Connected();
    void Read_FromServer();
    void Send_cmd();

    void find_or_create_Tab(const QString& Qstr);
    void find_or_create_Tab(const char *str);
    void new_tab(QListWidgetItem* item);
    void del_tab(int idx);


public:
    void del_tab0_and_enable_mbx();
    void Delay_ms(unsigned int ms)
    {
        QEventLoop myloop;
        QTimer::singleShot(ms, &myloop, SLOT(quit()));
        myloop.exec();
    }
    MainWindow(bool *pflag, QWidget *parent = 0);
    ~MainWindow()
    {
        delete      pLogDia;
        delete      sock;
        delete[]    ibuf;
    }
};


#endif // MAINWINDOW_H
