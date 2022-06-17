#include "logindialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>


QString cnQStr_TcpConnected     = QString::fromUtf8(u8"TCP: 已连接至服务器");
QString cnQStr_TcpDisconnected  = QString::fromUtf8(u8"TCP: 未连接到服务器");
QString cnQStr_InputIsNull      = QString::fromUtf8(u8"错误: 信息为空");
QString cnQStr_NotesForID       = QString::fromUtf8(u8" 学号/电话/识别码");
QString cnQStr_NotesForPWD      = QString::fromUtf8(u8" 包含数字和字母");
QString cnQStr_ReadyToSubmit    = QString::fromUtf8(u8"提示: 等待提交");
QString cnQStr_LoggingIn        = QString::fromUtf8(u8"正在登录 ...");
QString cnQStr_Registering      = QString::fromUtf8(u8"正在注册 ...");

/* English Version */
QString QStr_TcpConnected   = QString("TCP: is Connected.");
QString QStr_TcpDisconnected= QString("TCP: Disconnected!");
QString QStr_InputIsNull    = QString("ERR: Input is blank");
QString QStr_NotesForID     = QString(" Phone or Stu number");
QString QStr_NotesForPWD    = QString(" Contains 0-9, alpha");
QString QStr_ReadyToSubmit  = QString("NOTE: Ready to submit");
QString QStr_LoggingIn      = QString("Logging in ...");
QString QStr_Registering    = QString("Registering ...");
QString QStr_Correct        = QString("Correct");
QString QStr_Missed         = QString("NOTE: Wrong id/name");
QString QStr_Wrong          = QString("NOTE: Wrong password");



LoginDialog::LoginDialog(bool *pflag, QTcpSocket *sock, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    /* 允许点击Dialog窗体以获得Focus,因此点击窗体后会使LinEdit丢失光标 */
    this->vChinese = false;
    this->setFocusPolicy(Qt::ClickFocus);
    this->pflag_t = pflag;
    this->sock = sock;

    label1 = new QLabel(QString("Identity"));
    label1->setFixedWidth(70);
    label1->setAlignment(Qt::AlignCenter);
    lineEdit1 = new QLineEdit;
    lineEdit1->setPlaceholderText(QStr_NotesForID);
    lineEdit1->setFixedWidth(270);
    lineEdit1->setMaxLength(9);
    label1->setBuddy(lineEdit1);

    label2 = new QLabel(QString("Password"));
    label2->setFixedWidth(70);
    label2->setAlignment(Qt::AlignCenter);
    lineEdit2 = new QLineEdit;
    lineEdit2->setPlaceholderText(QStr_NotesForPWD);
    lineEdit2->setFixedWidth(270);
    lineEdit2->setEchoMode(QLineEdit::Password);
    lineEdit2->setMaxLength(20);
    label2->setBuddy(lineEdit2);

    chkbox_language = new QCheckBox(QString("Chinese"));
    chkbox_language->setEnabled(false);

    button_quit = new QPushButton(QString("Close"));
    button_quit->setEnabled(false);
    button_quit->setFixedWidth(80);
    button_register = new QPushButton(QString("Register"));
    button_register->setEnabled(false);
    button_register->setFixedWidth(80);
    button_login = new QPushButton(QString("Log in"));
    button_login->setEnabled(false);
    button_login->setFixedWidth(80);
    button_login->setDefault(true);//default button when press Enter

    label_tcpconn = new QLabel(QStr_TcpDisconnected);
    label_tcpconn->setAlignment(Qt::AlignLeft);
    label_tcpconn->setFixedWidth(170);
    label_status = new QLabel(QStr_InputIsNull);
    label_status->setAlignment(Qt::AlignRight);
    label_status->setFixedWidth(180);

    /* control of base components */
    connect(lineEdit1, SIGNAL(textChanged(QString)), this, SLOT(Enable_Buttons()));
    connect(lineEdit2, SIGNAL(textChanged(QString)), this, SLOT(Enable_Buttons()));
    connect(button_quit, SIGNAL(clicked(bool)), this, SLOT(close()));
    connect(chkbox_language, SIGNAL(stateChanged(int)), this, SLOT(Check_EnglishVer(int)));

    /* submit user-info to server */
    connect(button_register, SIGNAL(clicked(bool)), this, SLOT(Click_Register()));  // click
    connect(button_login, SIGNAL(clicked(bool)), this, SLOT(Click_Login()));        // click

    QHBoxLayout *toplayout = new QHBoxLayout;
    toplayout->addWidget(label1);
    toplayout->addWidget(lineEdit1);
    QHBoxLayout *midlayout = new QHBoxLayout;
    midlayout->addWidget(label2);
    midlayout->addWidget(lineEdit2);
    QHBoxLayout *botlayout = new QHBoxLayout;
    botlayout->addWidget(button_quit);
    botlayout->addWidget(button_register);
    botlayout->addWidget(button_login);
    QHBoxLayout *infolayout = new QHBoxLayout;
    infolayout->addWidget(label_tcpconn, 0, Qt::AlignLeft);
    infolayout->addWidget(label_status, 0, Qt::AlignRight);

    QVBoxLayout *mainlayout = new QVBoxLayout;
    mainlayout->addLayout(toplayout);
    mainlayout->addLayout(midlayout);
    mainlayout->addStretch(0);
    mainlayout->addWidget(chkbox_language, 100, Qt::AlignRight);
    mainlayout->addLayout(botlayout);
    mainlayout->addStretch(0);
    mainlayout->addLayout(infolayout);

    setLayout(mainlayout);
    this->setWindowTitle(QString("TheyChat"));
    this->setFixedSize(sizeHint().width(), 170);

    /* 暂时禁止两个LineEdit获得焦点,会在ReadyFor_Focus_Check()打开 */
    lineEdit1->setFocusPolicy(Qt::NoFocus);
    lineEdit2->setFocusPolicy(Qt::NoFocus);
}





// disable buttons in some cases
void LoginDialog::Enable_Buttons()
{
    /* 用户名不能包含空格, 密码不允许空格打头 */
    lineEdit1->setText(lineEdit1->text().remove(' '));
    QString temp = lineEdit2->text();
    while(temp.startsWith(' '))
        temp.remove(0, 1);
    lineEdit2->setText(temp);

    if(lineEdit1->text().isEmpty() || lineEdit2->text().isEmpty()) {
        button_login->setEnabled(false);
        button_register->setEnabled(false);
        if(vChinese)    label_status->setText(cnQStr_InputIsNull);
        else            label_status->setText(QStr_InputIsNull);
    }
    else {
        if(sock->state() == QAbstractSocket::UnconnectedState) {
            button_login->setEnabled(true);
            button_register->setEnabled(true);
//            button_login->setEnabled(false);
//            button_register->setEnabled(false);
        }
        else {
            button_login->setEnabled(true);
            button_register->setEnabled(true);
        }
        if(vChinese)    label_status->setText(cnQStr_ReadyToSubmit);
        else            label_status->setText(QStr_ReadyToSubmit);
    }
}




// if click button "Register"
void LoginDialog::Click_Register()
{
    if(vChinese)        label_status->setText(cnQStr_Registering);
    else                label_status->setText(QStr_Registering);
    Delay_ms(100);

    QString Qstr = QString("REG ") + lineEdit1->text() + " " + lineEdit2->text();
    char buf[1024] = {0};
    strcpy(buf, Qstr.toStdString().c_str());
    sock->write(buf, strlen(buf));







    *pflag_t = true;
    this->Delay_ms(200);
    this->close();
}



// if click button "Log in"
void LoginDialog::Click_Login()
{
    if(vChinese)        label_status->setText(cnQStr_LoggingIn);
    else                label_status->setText(QStr_LoggingIn);
    Delay_ms(100);

    QString Qstr = QString("LOG ") + lineEdit1->text().toUtf8().data() + " " + lineEdit2->text();
    char buf[1024] = {0};
    strcpy(buf, Qstr.toStdString().c_str());
    sock->write(buf, strlen(buf));
}




// show notes when just connected
void LoginDialog::LoginDialog::When_Connected()
{
    if(vChinese)    label_tcpconn->setText(cnQStr_TcpConnected);
    else            label_tcpconn->setText(QStr_TcpConnected);
}



// read the response from server
int LoginDialog::Read_FromServer(char *ibuf, QString& Qstr, QString& Qstr_idnum)
{
    //qDebug() << "read from server once";
    qint64 br = sock->read(ibuf, 4095);
    ibuf[br] = '\0';
    if(ibuf[br-1] == '\n')
        ibuf[br-1] = '\0';
    Qstr = QString::fromUtf8(ibuf);

    if(*pflag_t == false)
    {
        if(Qstr.startsWith("correct", Qt::CaseSensitive)) {
            char idnum[61] = {0};
            sscanf(ibuf, "%*s %s", idnum);
            Qstr_idnum = idnum;
            label_status->setText(QStr_Correct);
            *pflag_t = true;
            this->Delay_ms(200);
            this->close();
        }
        else if(Qstr.startsWith("missed", Qt::CaseInsensitive))
            label_status->setText(QStr_Missed);
        else if(Qstr.startsWith("wrong", Qt::CaseInsensitive))
            label_tcpconn->setText(QStr_Wrong);
        else
            label_tcpconn->setText(Qstr);
        return 0;
    }
    else
    {
        return 1;
    }
/*
    disconnect(sock, SIGNAL(connected()), this, SLOT(onConnected()));
    disconnect(sock, SIGNAL(readyRead()), this, SLOT(Read_Server()));
    sock->disconnectFromHost();
    sock->waitForDisconnected(100);
*/
}





// if 'checked', clear input and change into English version
void LoginDialog::Check_EnglishVer(int state)
{
    chkbox_language->clearFocus();

    if(state == Qt::Checked)
    {}
#if 0
    if(state == Qt::Checked)
    {
        this->vChinese = true;
        lineEdit1->setPlaceholderText(cnQStr_NotesForID);
        lineEdit2->setPlaceholderText(cnQStr_NotesForPWD);
        lineEdit1->clear();
        lineEdit2->clear();
        label1->setText(QString::fromUtf8(u8"账号"));
        label2->setTextQString::fromUtf8(u8"密码"));
        label_status->setText(cnQStr_InputIsNull);
        if(sock->state() == QAbstractSocket::UnconnectedState)
            label_tcpconn->setText(cnQStr_TcpDisconnected);
        else
            label_tcpconn->setText(cnQStr_TcpConnected);
    }
    else if(state == Qt::Unchecked)
    {
        this->vChinese = false;
        lineEdit1->setPlaceholderText(QStr_NotesForID);
        lineEdit2->setPlaceholderText(QStr_NotesForPWD);
        lineEdit1->clear();
        lineEdit2->clear();
        label1->setText("Identity");
        label2->setText("Password");
        label_status->setText(QStr_InputIsNull);
        if(sock->state() == QAbstractSocket::UnconnectedState)
            label_tcpconn->setText(QStr_TcpDisconnected);
        else
            label_tcpconn->setText(QStr_TcpConnected);
    }
    else
    {}
#endif
}




// set LineEdit(focus) and CheckBox(Enable) if ready
void LoginDialog::ReadyFor_Focus_Check()
{
    button_quit->setEnabled(true);
    chkbox_language->setEnabled(true);
    lineEdit1->setFocusPolicy(Qt::StrongFocus);
    lineEdit2->setFocusPolicy(Qt::StrongFocus);
    lineEdit1->setFocus();
}
