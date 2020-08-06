TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        commen/commen.cpp \
        httpconn/http_conn.cpp \
        httpserver/http_epoller.cpp \
        httpserver/http_server.cpp \
        main.cpp

HEADERS += \
    commen/commen.h \
    httpconn/http_conn.h \
    httpserver/http_epoller.h \
    httpserver/http_server.h
