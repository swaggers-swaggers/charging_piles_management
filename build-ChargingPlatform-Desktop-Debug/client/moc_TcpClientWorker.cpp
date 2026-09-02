/****************************************************************************
** Meta object code from reading C++ file 'TcpClientWorker.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../code/client/network/TcpClientWorker.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TcpClientWorker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_TcpClientWorker_t {
    const uint offsetsAndSize[32];
    char stringdata0[155];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_TcpClientWorker_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_TcpClientWorker_t qt_meta_stringdata_TcpClientWorker = {
    {
QT_MOC_LITERAL(0, 15), // "TcpClientWorker"
QT_MOC_LITERAL(16, 13), // "connectResult"
QT_MOC_LITERAL(30, 0), // ""
QT_MOC_LITERAL(31, 2), // "ok"
QT_MOC_LITERAL(34, 5), // "error"
QT_MOC_LITERAL(40, 11), // "requestDone"
QT_MOC_LITERAL(52, 4), // "type"
QT_MOC_LITERAL(57, 5), // "reply"
QT_MOC_LITERAL(63, 12), // "pushReceived"
QT_MOC_LITERAL(76, 3), // "msg"
QT_MOC_LITERAL(80, 18), // "socketDisconnected"
QT_MOC_LITERAL(99, 15), // "connectToServer"
QT_MOC_LITERAL(115, 9), // "doRequest"
QT_MOC_LITERAL(125, 7), // "payload"
QT_MOC_LITERAL(133, 9), // "timeoutMs"
QT_MOC_LITERAL(143, 11) // "onReadyRead"

    },
    "TcpClientWorker\0connectResult\0\0ok\0"
    "error\0requestDone\0type\0reply\0pushReceived\0"
    "msg\0socketDisconnected\0connectToServer\0"
    "doRequest\0payload\0timeoutMs\0onReadyRead"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_TcpClientWorker[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   56,    2, 0x06,    1 /* Public */,
       5,    2,   61,    2, 0x06,    4 /* Public */,
       8,    1,   66,    2, 0x06,    7 /* Public */,
      10,    0,   69,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      11,    0,   70,    2, 0x0a,   10 /* Public */,
      12,    3,   71,    2, 0x0a,   11 /* Public */,
      15,    0,   78,    2, 0x08,   15 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,    3,    4,
    QMetaType::Void, QMetaType::Int, QMetaType::QJsonObject,    6,    7,
    QMetaType::Void, QMetaType::QJsonObject,    9,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QJsonObject, QMetaType::Int,    6,   13,   14,
    QMetaType::Void,

       0        // eod
};

void TcpClientWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<TcpClientWorker *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connectResult((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->requestDone((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 2: _t->pushReceived((*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 3: _t->socketDisconnected(); break;
        case 4: _t->connectToServer(); break;
        case 5: _t->doRequest((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3]))); break;
        case 6: _t->onReadyRead(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (TcpClientWorker::*)(bool , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&TcpClientWorker::connectResult)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (TcpClientWorker::*)(int , const QJsonObject & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&TcpClientWorker::requestDone)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (TcpClientWorker::*)(const QJsonObject & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&TcpClientWorker::pushReceived)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (TcpClientWorker::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&TcpClientWorker::socketDisconnected)) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject TcpClientWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_TcpClientWorker.offsetsAndSize,
    qt_meta_data_TcpClientWorker,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_TcpClientWorker_t
, QtPrivate::TypeAndForceComplete<TcpClientWorker, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<QJsonObject, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>


>,
    nullptr
} };


const QMetaObject *TcpClientWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TcpClientWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_TcpClientWorker.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TcpClientWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void TcpClientWorker::connectResult(bool _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void TcpClientWorker::requestDone(int _t1, const QJsonObject & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void TcpClientWorker::pushReceived(const QJsonObject & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void TcpClientWorker::socketDisconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
