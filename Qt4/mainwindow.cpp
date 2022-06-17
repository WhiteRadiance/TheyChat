#include "mainwindow.h"
#include <stdio.h>


MainWindow::MainWindow(bool *pflag, QWidget *parent)
    : QMainWindow(parent)
{
    QString HostAddr = "0.0.0.0";
    quint16 HostPort = 6666;

    ibuf = new char[BUFFER_SIZE]();

    sock = new QTcpSocket;
    connect(sock, SIGNAL(connected()), this, SLOT(When_Connected()));
    connect(sock, SIGNAL(readyRead()), this, SLOT(Read_FromServer()));

    pLogDia = new LoginDialog(pflag, sock);

    sock->connectToHost(HostAddr, HostPort);
    sock->waitForConnected(150); // wait for 150ms

    pLogDia->show();
    pLogDia->Delay_ms(340);
    pLogDia->ReadyFor_Focus_Check();

//==============================================================
    Dock0 = new QDockWidget("   List of friends", this);
    Dock1 = new QDockWidget(this);

    Dock0->setFeatures(QDockWidget::NoDockWidgetFeatures);
    Dock1->setFeatures(QDockWidget::NoDockWidgetFeatures);
    this->addDockWidget(Qt::RightDockWidgetArea, Dock0);
    this->addDockWidget(Qt::LeftDockWidgetArea, Dock1);//DockWidgetClosable

    /* 删除Dock1,2的标题栏 */
    QWidget *nullTitle = new QWidget(this);
    Dock1->setTitleBarWidget(nullTitle);

    /* 为Dock0添加好友列表 */
    list = new subwin_list(Dock0);
    Dock0->setWidget(list);

    /* 为Dock1添加文本界面 */
    tabWidget = new TabWidget(Dock1);
    tabWidget->setTabsClosable(true);
    Dock1->setWidget(tabWidget);
    tab0 = new subwin_text(tabWidget, true);//a empty tab

    tabWidget->addTab(tab0, "Tab0");
    tab0->id = "Tab0";
    mTab["Tab0"] = tab0;

    connect(list->listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(new_tab(QListWidgetItem*)));
    connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(del_tab(int)));

    this->resize(680, 370);
}



void MainWindow::When_Connected()
{
    pLogDia->When_Connected();
}



void MainWindow::Read_FromServer()
{
    if(1 == pLogDia->Read_FromServer(ibuf, Qstr_response, Qstr_idnum))
    {
        subwin_text *ptr = nullptr;

        if(Qstr_response.startsWith("RAW ")) {
            char sender[61] = {0};
            sscanf(ibuf, "%*s %s", sender);
            this->find_or_create_Tab(sender);
            QString real_msg = QString::fromUtf8(ibuf+4+strlen(sender)+1+Qstr_idnum.size()+1);

            mTab[QString(sender)]->textEdit->append(real_msg);
            mTab[QString(sender)]->textEdit->setAlignment(Qt::AlignLeft);
        }

        ptr = (subwin_text*)(tabWidget->currentWidget());
        ptr->lineEdit->setFocus();
    }
}



void MainWindow::Send_cmd()
{
    subwin_text *ptr = (subwin_text*)(tabWidget->currentWidget());
    if(ptr->lineEdit->text().size() > 0)
    {
        QString QStr_cmd = "SND RAW " + Qstr_idnum + " " + ptr->id + \
                " " + ptr->lineEdit->text().toUtf8().data();
        char buf[1024] = {0};
        strcpy(buf, QStr_cmd.toStdString().c_str());
        if(sock->state() == QAbstractSocket::ConnectedState) {
            sock->write(buf, strlen(buf));

            ptr->textEdit->append(ptr->lineEdit->text());
            ptr->textEdit->setAlignment(Qt::AlignRight);
            ptr->lineEdit->clear();
        }
    }
}



void MainWindow::find_or_create_Tab(const QString& Qstr)
{
    if(mTab.find(Qstr) == mTab.end())
    {
        mTab[Qstr] = new subwin_text(tabWidget);
        mTab[Qstr]->id = Qstr;
        connect(mTab[Qstr]->lineEdit, SIGNAL(returnPressed()), this, SLOT(Send_cmd()));
        tabWidget->addTab(mTab[Qstr], Qstr);
    }
}



void MainWindow::find_or_create_Tab(const char *str)
{
    QString Qstr = str;
    this->find_or_create_Tab(Qstr);
}



void MainWindow::new_tab(QListWidgetItem* item)
{
    this->find_or_create_Tab(item->text());

    /* 进入新增标签页 */
    tabWidget->setCurrentWidget(mTab[item->text()]);
    mTab[item->text()]->lineEdit->setFocus();
}



void MainWindow::del_tab(int idx)
{
    map<QString, subwin_text*>::iterator it;
    for(it=mTab.begin(); it!=mTab.end(); it++)
    {
        if(tabWidget->indexOf(it->second) == idx)
        {
            QString tab_name = it->second->id;
            disconnect(it->second->lineEdit, SIGNAL(returnPressed()), this, SLOT(Send_cmd()));
            mTab.erase(tab_name);
            tabWidget->removeTab(idx);
            break;
        }
    }
}




void MainWindow::del_tab0_and_enable_mbx()
{
    //qDebug() << "delete Tab0";
    if(mTab.begin()->second->EmpWid != nullptr)
    {
        mTab.erase(mTab.begin());
        tabWidget->removeTab(0);
    }

    /* 读取可能出现的留言邮件(只实现了读取一封邮件)
       也许将信件内容的\n编码为\a或其他无效字符,
       然后可以勉强使用\n来分割多封信件. */
    char buf[] = "SND RDY4MBX";
    sock->write(buf, strlen(buf));
}

