#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QEventLoop>
#include <QTimer>
#include <QTcpSocket>


class LoginDialog : public QDialog
{
    Q_OBJECT

public slots:
    void Enable_Buttons();
    void Click_Register();
    void Click_Login();
    void When_Connected();
    int Read_FromServer(char *ibuf, QString& Qstr, QString& Qstr_idnum);
    void Check_EnglishVer(int state);
    void ReadyFor_Focus_Check();

    void Delay_ms(unsigned int ms)
    {
        QEventLoop myloop;
        QTimer::singleShot(ms, &myloop, SLOT(quit()));
        myloop.exec();
    }


private:
    bool            *pflag_t;
    QTcpSocket      *sock;
    bool             vChinese;

    QLineEdit       *lineEdit1;
    QLineEdit       *lineEdit2;
    QLabel          *label1;
    QLabel          *label2;
    QPushButton     *button_quit;
    QPushButton     *button_login;
    QPushButton     *button_register;
    QCheckBox       *chkbox_language;
    QLabel          *label_tcpconn;
    QLabel          *label_status;


public:
    LoginDialog(bool *pflag, QTcpSocket *sock, QWidget *parent=0, Qt::WindowFlags f=0);
    ~LoginDialog()
    {
        delete lineEdit1;
        delete lineEdit2;
        delete label1;
        delete label2;
        delete button_quit;
        delete button_login;
        delete button_register;
        delete chkbox_language;
        delete label_tcpconn;
        delete label_status;
    }
};


#endif // LOGINDIALOG_H
