TEMPLATE = app

include( ../../version.pri )

RESOURCES = paint.qrc

TARGET = 
DEPENDPATH += .

QT += opengl
QT += widgets core gui xml

CONFIG += release

DESTDIR = ../../bin

TARGET = drishtipaint

INCLUDEPATH += graphcut

# Input
FORMS += drishtipaint.ui viewermenu.ui graphcutmenu.ui curvesmenu.ui fibersmenu.ui propertyeditor.ui

INCLUDEPATH += c:\Qt\libQGLViewer-2.6.1
LIBS += QGLViewer2.lib
QMAKE_LIBDIR += c:\Qt\libQGLViewer-2.6.1\lib

HEADERS += commonqtclasses.h \
	drishtipaint.h \
	bitmapthread.h \
	curvegroup.h \
	dcolordialog.h \
	dcolorwheel.h \
	fiber.h \
	fibergroup.h\
	imagewidget.h \
	global.h \
	gradienteditor.h \
	gradienteditorwidget.h \
        livewire.h \
	myslider.h \
 	morphcurve.h \
	propertyeditor.h \
	splineeditor.h \
	splineeditorwidget.h \
	splineinformation.h \
	splinetransferfunction.h \
	staticfunctions.h \
	transferfunctioncontainer.h \
	transferfunctioneditorwidget.h \
	transferfunctionmanager.h \
	tagcoloreditor.h \
	coloreditor.h \
	opacityeditor.h \
	viewer.h \
	volume.h \
	volumefilemanager.h \
	volumemask.h \
	graphcut/graph.h \
	graphcut/graphcut.h \
	graphcut/block.h \
	graphcut/point.h \
	ply.h \
	lookuptable.h \
	marchingcubes.h


SOURCES += drishtipaint.cpp \
	main.cpp \
	bitmapthread.cpp \
	curvegroup.cpp \
	dcolordialog.cpp \
	dcolorwheel.cpp \
	fiber.cpp \
	fibergroup.cpp\
	imagewidget.cpp \
	global.cpp \
	gradienteditor.cpp \
	gradienteditorwidget.cpp \
        livewire.cpp \
	myslider.cpp \
 	morphcurve.cpp \
	propertyeditor.cpp \
	splineeditor.cpp \
	splineeditorwidget.cpp \
	splineinformation.cpp \
	splinetransferfunction.cpp \
	staticfunctions.cpp \
	transferfunctioncontainer.cpp \
	transferfunctioneditorwidget.cpp \
	transferfunctionmanager.cpp \
	tagcoloreditor.cpp \
	coloreditor.cpp \
	opacityeditor.cpp \
	viewer.cpp \
	volume.cpp \
	volumefilemanager.cpp \
	volumemask.cpp \
	graphcut/graph.cpp \
	graphcut/graphcut.cpp \
	ply.c \
	marchingcubes.cpp
