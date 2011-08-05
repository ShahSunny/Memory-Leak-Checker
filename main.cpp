#include "LeakChecker.h"
//#include <QtCore>
#include <execinfo.h>

struct TestStruct{
    int a;
    int b;
    TestStruct() {
        a = 0;
        b = 0;

    }
    ~TestStruct() {

    }
    void myTestCallOne();

    void myTestCallTwo();

    void myTestCallThree();

    void myTestCallFour();

};

void TestStruct::myTestCallOne() {
    myTestCallTwo();
}

void TestStruct::myTestCallTwo() {
    myTestCallThree();
    for(int i=0; i<100; i++) {
        TestStruct *d = new TestStruct[10000];
        delete[] d;
        d = new TestStruct();
        delete d;
    }
}

void TestStruct::myTestCallThree() {
    for(int i=0; i<20; i++)
        new int[2];
    myTestCallFour();
}


void TestStruct::myTestCallFour() {
    for(int i=0; i<100; i++)
        new TestStruct[30];

}

int main(int argc, char *argv[])
{
   // QCoreApplication a(argc, argv);
    char *arr = new char[100];
    delete[] arr;
    arr = new char();
    delete arr;
    TestStruct *arrTestStruct = new TestStruct();
    TestStruct testStruct;
    testStruct.myTestCallOne();

    //QTimer t;
    //t.singleShot(100,&a,SLOT(quit()));
    //a.exec();
    return 1;
}
