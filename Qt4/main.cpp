#include <QDialog>
#include "logindialog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDesktopWidget>


int main(int argc, char *argv[])
{
    bool flag_normal = false;   // dialog closed abnormally

    QApplication app(argc, argv);
    MainWindow win(&flag_normal);

    /* blocking until closed */
    win.pLogDia->exec();
    if(flag_normal == false)
        exit(-1);

    /* 把Widget显示在屏幕的正中间, 并删除Tab0 */
    int currentScreen = app.desktop()->screenNumber(&win);      //屏幕(副屏)所在的编号
    QRect rect = app.desktop()->screenGeometry(currentScreen);  //获得所在屏幕的尺寸
    win.move((rect.width() - win.width()) / 2, (rect.height() - win.height()) / 2);
    win.show();
    win.del_tab0_and_enable_mbx();

    return app.exec();
}
