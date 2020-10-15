#include "corrector_control.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    correctorControl w;
    w.show();

    return a.exec();
}
