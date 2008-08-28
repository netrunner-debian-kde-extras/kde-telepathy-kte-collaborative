#ifndef IOQSOCKETNOTIFIER_H
#define IOQSOCKETNOTIFIER_H

#include <libinfinitymm/common/io.h>
#include <glibmm/object.h>

#include <QSocketNotifier>

namespace Infinity
{

class IoQSocketNotifier
    : public QSocketNotifier
{
    Q_OBJECT

public:
    static QSocketNotifier::Type convertEventType(IoEvent event);
    static IoEvent convertEventType(QSocketNotifier::Type event);
    static const QString typeString(QSocketNotifier::Type type);

public:
    IoQSocketNotifier( int socket,
        QSocketNotifier::Type type,
        IoFunction handler_func,
        void *user_data,
        Glib::Object::DestroyNotify destroy_notifiy,
        QObject *parent = 0
    );
    ~IoQSocketNotifier();
    
    void *getUserData();

private Q_SLOTS:
    void slotActivated( int socket );

private:
    int socket_desc; // This is redundant, but inf_io_watch wants a socket pointer.
    IoFunction handler;
    void *user_data;
    Glib::Object::DestroyNotify destroy_notify;

}; // class SocketNotifier

} // namespace Infinity

#endif
