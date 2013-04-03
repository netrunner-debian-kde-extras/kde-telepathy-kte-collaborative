/*  This file is part of kobby
    Copyright (c) 2013 Sven Brauch <svenbrauch@gmail.com>


    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kio_infinity.h"

#include <kdebug.h>
#include <kcomponentdata.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <kencodingprober.h>
#include <qcoreapplication.h>
#include <qapplication.h>

#include <common/itemfactory.h>
#include <common/noteplugin.h>
#include <common/documentbuilder.h>
#include <remotebrowserview.h>
#include <libqinfinity/browsermodel.h>
#include <libqinfinity/browser.h>
#include <libqinfinity/xmlconnection.h>
#include <libqinfinity/xmppconnection.h>
#include <libqinfinity/init.h>

#include "malloc.h"
#include <KTextEditor/View>

#include "common/utils.h"

using namespace KIO;

extern "C" {

int KDE_EXPORT kdemain( int argc, char **argv )
// int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
//     QApplication app(argc, argv);
    KComponentData componentData("infinity", "kio_infinity");

    qDebug() << "starting infinity kioslave";
    if (argc != 4) {
        qDebug() << "wrong arguments count";
        exit(-1);
    }

    QInfinity::init();

    InfinityProtocol slave(argv[2], argv[3]);
    slave.dispatchLoop();
//     slave.listDir(KUrl("inf://localhost"));

    qDebug() << "slave exiting";
    return app.exec();
}

}

InfinityProtocol* InfinityProtocol::_self = 0;

InfinityProtocol::InfinityProtocol(const QByteArray &pool_socket, const QByteArray &app_socket)
    : QObject(), SlaveBase("inf", pool_socket, app_socket)
{
    qDebug() << "constructing infinity kioslave";
    _self = this;
}

InfinityProtocol* InfinityProtocol::self()
{
    return _self;
}

InfinityProtocol::~InfinityProtocol()
{
    _self = 0;
}

void InfinityProtocol::get(const KUrl& url )
{
    kDebug() << "GET " << url.url();

    QString title, section;

    // tell the mimetype
    mimeType("text/plain");
    data("");
    finished();
}

void InfinityProtocol::stat( const KUrl& url)
{
    kDebug() << "ENTERING STAT " << url.url();

    UDSEntry entry;
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("text/plain"));
    statEntry(entry);

    finished();
}

void InfinityProtocol::doConnect(const Peer& peer)
{
    if ( m_connectedTo == peer ) {
        // TODO check if connection is still open
        return;
    }

    QEventLoop loop;
    m_connection = QSharedPointer<Kobby::Connection>(new Kobby::Connection(peer.hostname, peer.port, this));
    m_browserModel = QSharedPointer<QInfinity::BrowserModel>(new QInfinity::BrowserModel( this ));
    m_browserModel->setItemFactory( new Kobby::ItemFactory( this ) );
    QObject::connect(m_connection.data(), SIGNAL(ready(Connection*)), &loop, SLOT(quit()));
    m_connection->prepare();
    m_connectedTo = peer;

    m_notePlugin = QSharedPointer<Kobby::NotePlugin>(new Kobby::NotePlugin( this ));
    m_browserModel->addPlugin( *m_notePlugin );

    // TODO make synchronous properly
    loop.exec();
    m_browserModel->addConnection(static_cast<QInfinity::XmlConnection*>(m_connection->xmppConnection()), "kio_root");
    m_connection->open();

    kDebug() << "connection status:" << m_connection->xmppConnection()->status() << QInfinity::XmlConnection::Open;

    // browsers.first is ok, since we only have one connection
    QInfinity::Browser* browser = m_browserModel->browsers().first();
    while ( browser->connectionStatus() != INFC_BROWSER_CONNECTED ) {
        QCoreApplication::processEvents();
    }
}


void InfinityProtocol::mimetype(const KUrl & /*url*/)
{
    mimeType("text/plain");
    finished();
}

void InfinityProtocol::put(const KUrl& url, int /*permissions*/, JobFlags /*flags*/)
{
    kDebug() << "PUT" << url;
    doConnect(Peer(url.host(), url.port()));
    QInfinity::BrowserIter iter = iterForUrl(url.upUrl());
    kDebug() << "adding note" << iter.path() << url.fileName();
    browser()->addNote(iter, url.fileName().toAscii().data(), *m_notePlugin, false);
    // TODO error handling and waiting
    finished();
}

void InfinityProtocol::mkdir(const KUrl& url, int /*permissions*/)
{
    kDebug() << "MKDIR" << url;
    doConnect(Peer(url.host(), url.port()));
    QInfinity::BrowserIter iter = iterForUrl(url.upUrl());
    browser()->addSubdirectory(iter, url.fileName().toAscii().data());
    // TODO error handling and waiting
    finished();
}

QInfinity::BrowserIter InfinityProtocol::iterForUrl(const KUrl& url)
{
    KUrl clean(url);
    clean.cleanPath(KUrl::SimplifyDirSeparators);
    IterLookupHelper helper(clean.path(KUrl::AddTrailingSlash), browser());
    QEventLoop loop;
    kDebug() << "connecting signal:" << connect(&helper, SIGNAL(done(QInfinity::BrowserIter)), &loop, SLOT(quit()));
    helper.beginLater();
    // Using an event loop is okay in this case, because the kio slave doesn't get
    // any signals from outside.
    loop.exec();
    kDebug() << "ok, found iter";
    return helper.result();
}

void InfinityProtocol::listDir(const KUrl &url)
{
    kDebug() << "LIST DIR" << url;
    kDebug() << url.host() << url.userName() << url.password() << url.path();

    doConnect(Peer(url.host(), url.port()));

    QInfinity::BrowserIter iter = iterForUrl(url);

    if ( ! iter.isExplored() ) {
        kDebug() << "exploring iter";
        InfcExploreRequest* request = iter.explore();
        while ( INFC_IS_EXPLORE_REQUEST(request) && ! infc_explore_request_get_finished(INFC_EXPLORE_REQUEST(request)) ) {
            kDebug() << "waiting for exploration";
            QCoreApplication::processEvents();
        }
    }
    bool hasChildren = iter.child();

    // If not, the directory is just empty.
    if ( hasChildren ) {
        do {
            UDSEntry entry;
            entry.insert( KIO::UDSEntry::UDS_URL, url.url(KUrl::AddTrailingSlash) + iter.name() );
            entry.insert( KIO::UDSEntry::UDS_NAME, iter.name() );
            entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, iter.isDirectory() ? S_IFDIR : S_IFREG );
            kDebug() << "listing" << iter.path();
            listEntry(entry, false);
        } while ( iter.next() );
    }

    listEntry(UDSEntry(), true);
    finished();
}

QInfinity::Browser* InfinityProtocol::browser() const
{
    return m_browserModel->browsers().first();
}

