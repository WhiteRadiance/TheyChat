#include "dockwindow.h"
#include <QVBoxLayout>


subwin_text::subwin_text(QWidget *parent, bool empty)
    : QWidget(parent)
{
    QVBoxLayout *mainlayout = new QVBoxLayout;

    if(!empty) {
        EmpWid = nullptr;
        textEdit = new QTextEdit();
        textEdit->setText("");
        textEdit->setReadOnly(true);
        lineEdit = new QLineEdit();
        lineEdit->setText("");

        connect(lineEdit, SIGNAL(textChanged(QString)), this, SLOT(lineEdit_limit()));

        mainlayout->addWidget(textEdit);
        mainlayout->addWidget(lineEdit);
    }
    else {
        textEdit = nullptr;
        lineEdit = nullptr;
        EmpWid = new QWidget();

        mainlayout->addWidget(EmpWid);
    }
    this->setLayout(mainlayout);

    this->setMinimumSize(170, 200);
}



void subwin_text::lineEdit_limit()
{
    /* 禁止由空格开头 */
    QString temp = lineEdit->text();
    while(temp.startsWith(' '))
        temp.remove(0, 1);
    lineEdit->setText(temp);
}




subwin_list::subwin_list(QWidget *parent)
    : QWidget(parent)
{
    listWidget = new QListWidget(parent);
    listWidget->addItem("S200101046");
    listWidget->addItem("S200101174");
    listWidget->addItem("S200131146");
    listWidget->addItem("S200131175");
    listWidget->addItem("S200131186");
    listWidget->addItem("S200131278");
    listWidget->addItem("S200131317");

    listWidget->addItem("          ");
    listWidget->addItem("          ");
    listWidget->addItem("          ");

    listWidget->addItem("S200101046");
    listWidget->addItem("S200101174");
    listWidget->addItem("S20010----");
    listWidget->addItem("S20010++++");
    listWidget->addItem("S20010****");
    listWidget->addItem("S20010////");
    listWidget->addItem("S20010====");
    listWidget->addItem("S20010<<<<");
    listWidget->addItem("S20010>>>>");
    listWidget->addItem("S20010....");
    listWidget->addItem("S20010????");
    listWidget->addItem("S20010####");
    listWidget->addItem("S20010$$$$");
    listWidget->addItem("S20010%%%%");
    listWidget->addItem("S20010&&&&");
    listWidget->addItem("S20010^^^^");
    listWidget->addItem("S20010____");
    listWidget->addItem("S20010||||");
    listWidget->addItem("S20010~~~~");

    QVBoxLayout *mainlayout = new QVBoxLayout;
    //mainlayout->addSpacing(1);
    mainlayout->addWidget(listWidget);
    this->setLayout(mainlayout);

    this->setMinimumSize(145, 200);
    this->setMaximumWidth(270);
}

