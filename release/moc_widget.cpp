/****************************************************************************
** Meta object code from reading C++ file 'widget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.10)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../widget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'widget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.10. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_C2hReaderWorker_t {
    QByteArrayData data[17];
    char stringdata0[150];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_C2hReaderWorker_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_C2hReaderWorker_t qt_meta_stringdata_C2hReaderWorker = {
    {
QT_MOC_LITERAL(0, 0, 15), // "C2hReaderWorker"
QT_MOC_LITERAL(1, 16, 10), // "frameReady"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 7), // "payload"
QT_MOC_LITERAL(4, 36, 5), // "width"
QT_MOC_LITERAL(5, 42, 6), // "height"
QT_MOC_LITERAL(6, 49, 9), // "workerLog"
QT_MOC_LITERAL(7, 59, 4), // "text"
QT_MOC_LITERAL(8, 64, 9), // "readError"
QT_MOC_LITERAL(9, 74, 4), // "code"
QT_MOC_LITERAL(10, 79, 7), // "stopped"
QT_MOC_LITERAL(11, 87, 5), // "start"
QT_MOC_LITERAL(12, 93, 8), // "quintptr"
QT_MOC_LITERAL(13, 102, 14), // "c2hHandleValue"
QT_MOC_LITERAL(14, 117, 10), // "frameBytes"
QT_MOC_LITERAL(15, 128, 10), // "chunkBytes"
QT_MOC_LITERAL(16, 139, 10) // "throttleMs"

    },
    "C2hReaderWorker\0frameReady\0\0payload\0"
    "width\0height\0workerLog\0text\0readError\0"
    "code\0stopped\0start\0quintptr\0c2hHandleValue\0"
    "frameBytes\0chunkBytes\0throttleMs"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_C2hReaderWorker[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    3,   39,    2, 0x06 /* Public */,
       6,    1,   46,    2, 0x06 /* Public */,
       8,    1,   49,    2, 0x06 /* Public */,
      10,    0,   52,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      11,    6,   53,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QByteArray, QMetaType::Int, QMetaType::Int,    3,    4,    5,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 12, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   13,   14,   15,   16,    4,    5,

       0        // eod
};

void C2hReaderWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<C2hReaderWorker *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->frameReady((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        case 1: _t->workerLog((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->readError((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->stopped(); break;
        case 4: _t->start((*reinterpret_cast< quintptr(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< int(*)>(_a[4])),(*reinterpret_cast< int(*)>(_a[5])),(*reinterpret_cast< int(*)>(_a[6]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (C2hReaderWorker::*)(const QByteArray & , int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&C2hReaderWorker::frameReady)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (C2hReaderWorker::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&C2hReaderWorker::workerLog)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (C2hReaderWorker::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&C2hReaderWorker::readError)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (C2hReaderWorker::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&C2hReaderWorker::stopped)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject C2hReaderWorker::staticMetaObject = { {
    &QObject::staticMetaObject,
    qt_meta_stringdata_C2hReaderWorker.data,
    qt_meta_data_C2hReaderWorker,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *C2hReaderWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *C2hReaderWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_C2hReaderWorker.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int C2hReaderWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void C2hReaderWorker::frameReady(const QByteArray & _t1, int _t2, int _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)), const_cast<void*>(reinterpret_cast<const void*>(&_t3)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void C2hReaderWorker::workerLog(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void C2hReaderWorker::readError(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void C2hReaderWorker::stopped()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
struct qt_meta_stringdata_Widget_t {
    QByteArrayData data[13];
    char stringdata0[185];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Widget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Widget_t qt_meta_stringdata_Widget = {
    {
QT_MOC_LITERAL(0, 0, 6), // "Widget"
QT_MOC_LITERAL(1, 7, 22), // "on_btnOpenXdma_clicked"
QT_MOC_LITERAL(2, 30, 0), // ""
QT_MOC_LITERAL(3, 31, 25), // "on_btnRunSelfTest_clicked"
QT_MOC_LITERAL(4, 57, 26), // "on_btnStartReceive_clicked"
QT_MOC_LITERAL(5, 84, 25), // "on_btnStopReceive_clicked"
QT_MOC_LITERAL(6, 110, 18), // "onReaderFrameReady"
QT_MOC_LITERAL(7, 129, 7), // "payload"
QT_MOC_LITERAL(8, 137, 5), // "width"
QT_MOC_LITERAL(9, 143, 6), // "height"
QT_MOC_LITERAL(10, 150, 13), // "onReaderError"
QT_MOC_LITERAL(11, 164, 4), // "code"
QT_MOC_LITERAL(12, 169, 15) // "onReaderStopped"

    },
    "Widget\0on_btnOpenXdma_clicked\0\0"
    "on_btnRunSelfTest_clicked\0"
    "on_btnStartReceive_clicked\0"
    "on_btnStopReceive_clicked\0onReaderFrameReady\0"
    "payload\0width\0height\0onReaderError\0"
    "code\0onReaderStopped"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Widget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    0,   50,    2, 0x08 /* Private */,
       4,    0,   51,    2, 0x08 /* Private */,
       5,    0,   52,    2, 0x08 /* Private */,
       6,    3,   53,    2, 0x08 /* Private */,
      10,    1,   60,    2, 0x08 /* Private */,
      12,    0,   63,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QByteArray, QMetaType::Int, QMetaType::Int,    7,    8,    9,
    QMetaType::Void, QMetaType::Int,   11,
    QMetaType::Void,

       0        // eod
};

void Widget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Widget *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->on_btnOpenXdma_clicked(); break;
        case 1: _t->on_btnRunSelfTest_clicked(); break;
        case 2: _t->on_btnStartReceive_clicked(); break;
        case 3: _t->on_btnStopReceive_clicked(); break;
        case 4: _t->onReaderFrameReady((*reinterpret_cast< const QByteArray(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        case 5: _t->onReaderError((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->onReaderStopped(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject Widget::staticMetaObject = { {
    &QWidget::staticMetaObject,
    qt_meta_stringdata_Widget.data,
    qt_meta_data_Widget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *Widget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Widget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Widget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Widget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
