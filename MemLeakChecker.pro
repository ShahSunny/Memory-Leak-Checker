#-------------------------------------------------
#
# Project created by QtCreator 2011-07-23T19:57:54
#
#-------------------------------------------------

QT       -= core gui

QMAKE_LFLAGS += -rdynamic
TARGET = memleak-check
TEMPLATE = app


SOURCES += main.cpp \
    LeakChecker.cpp

HEADERS  += \
    LeakChecker.h
