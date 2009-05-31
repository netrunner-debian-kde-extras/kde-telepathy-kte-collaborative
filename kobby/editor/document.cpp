/*
 * Copyright 2009  Gregory Haynes <greg@greghaynes.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "document.h"
#include "kobbysettings.h"

#include <libqinfinity/sessionproxy.h>
#include <libqinfinity/session.h>
#include <libqinfinity/textsession.h>
#include <libqinfinity/userrequest.h>
#include <libqinfinity/adopteduser.h>
#include <libqinfinity/textbuffer.h>
#include <libqinfinity/textchunk.h>
#include <libqinfinity/browseriter.h>

#include <KTextEditor/Document>
#include <KTextEditor/Range>
#include <KTextEditor/Cursor>
#include <KTextEditor/View>
#include <KLocalizedString>
#include <KMessageBox>
#include <KDebug>

#include <QString>
#include <QTextCodec>
#include <QTextEncoder>
#include <QAction>

namespace Kobby
{

Document::Document( KTextEditor::Document &kDocument )
    : m_kDocument( &kDocument )
    , m_loadState( Document::Unloaded )
    , m_isCollaborative( false )
{
    m_kDocument->setParent( 0 );
}

Document::~Document()
{
    if( m_kDocument )
        delete m_kDocument.data();
}

KTextEditor::Document *Document::kDocument() const
{
    return m_kDocument;
}

bool Document::save()
{
    return m_kDocument->documentSave();
}

QString Document::name()
{
    return m_kDocument->documentName();
}

Document::LoadState Document::loadState() const
{
    return m_loadState;
}

void Document::setLoadState( Document::LoadState state )
{
    if( state != LoadState() )
    {
        m_loadState = state;
        emit( loadStateChanged( this, state ) );
        if( state == Document::Complete )
            emit( loadingComplete( this ) );
    }
}

void Document::throwFatalError( const QString &message )
{
    emit( fatalError( this, message ) );
    deleteLater();
}

void Document::setCollaborative(bool is_collaborative)
{
    m_isCollaborative = is_collaborative;
}


KDocumentTextBuffer::KDocumentTextBuffer( KTextEditor::Document &kDocument,
    const QString &encoding,
    QObject *parent )
    : QInfinity::AbstractTextBuffer( encoding, parent )
    , blockLocalInsert( false )
    , blockLocalRemove( false )
    , blockRemoteInsert( false )
    , blockRemoteRemove( false )
    , m_kDocument( &kDocument )
{
    connect( &kDocument, SIGNAL(textInserted(KTextEditor::Document*, const KTextEditor::Range&)),
        this, SLOT(localTextInserted(KTextEditor::Document*, const KTextEditor::Range&)) );
    connect( &kDocument, SIGNAL(textRemoved(KTextEditor::Document*, const KTextEditor::Range&)),
        this, SLOT(localTextRemoved(KTextEditor::Document*, const KTextEditor::Range&)) );
}

KDocumentTextBuffer::~KDocumentTextBuffer()
{
}

KTextEditor::Document *KDocumentTextBuffer::kDocument() const
{
    return m_kDocument;
}

void KDocumentTextBuffer::onInsertText( unsigned int offset,
    const QInfinity::TextChunk &chunk,
    QInfinity::User *user )
{
    Q_UNUSED(user)

    if( !blockRemoteInsert )
    {
        KTextEditor::Cursor startCursor = offsetToCursor( offset );
        blockLocalInsert = true;
        QString str = codec()->toUnicode( chunk.text() );
        kDocument()->insertText( startCursor, str );
    }
    else
        blockRemoteInsert = false;
}

void KDocumentTextBuffer::onEraseText( unsigned int offset,
    unsigned int length,
    QInfinity::User *user )
{
    Q_UNUSED(user)

    if( !blockRemoteRemove )
    {
        KTextEditor::Cursor startCursor = offsetToCursor( offset );
        KTextEditor::Cursor endCursor = offsetToCursor( offset+length );
        blockLocalRemove = true;
        kDocument()->removeText( KTextEditor::Range(startCursor, endCursor) );
    }
    else
        blockRemoteRemove = false;
}

void KDocumentTextBuffer::joinFailed( GError *error )
{
    QString errorString = i18n("Joining failed: ");
    errorString.append( error->message );
    KMessageBox::error( 0, errorString, i18n("Joining Failed") );
}

void KDocumentTextBuffer::localTextInserted( KTextEditor::Document *document,
    const KTextEditor::Range &range )
{
    Q_UNUSED(document)

    unsigned int offset;
    if( !blockLocalInsert )
    {
        if( !m_user.isNull() )
        {
            offset = cursorToOffset( range.start() );
            QInfinity::TextChunk chunk( encoding() );
            QString text = kDocument()->text( range );
            if( text[0] == '\n' ) // hack
                text = '\n';
            if( encoder() )
            {
                QByteArray encodedText = codec()->fromUnicode( text );
                chunk.insertText( 0, encodedText, text.length(), m_user->id() );
                blockRemoteInsert = true;
                insertChunk( offset, chunk, m_user );
            }
            else
                kDebug() << "No encoder for text codec.";
        }
        else
            kDebug() << "Could not insert text: No local user set.";
    }
    else
        blockLocalInsert = false;
}

void KDocumentTextBuffer::localTextRemoved( KTextEditor::Document *document,
    const KTextEditor::Range &range )
{
    Q_UNUSED(document)

    if( !blockLocalRemove )
    {
        if( !m_user.isNull() )
        {
            unsigned int offset = cursorToOffset( range.start() );
            unsigned int end = cursorToOffset( range.end() );
            blockRemoteRemove = true;
            eraseText( offset, end-offset, m_user );
        }
        else
            kDebug() << "Could not remove text: No local user set.";
    }
    else
        blockLocalRemove = false;
}

void KDocumentTextBuffer::setUser( QPointer<QInfinity::User> user )
{
    m_user = user;
}

unsigned int KDocumentTextBuffer::cursorToOffset( const KTextEditor::Cursor &cursor )
{
    unsigned int offset = 0;
    int i, cursor_line = cursor.line();
    for( i = 0; i < cursor_line; i++ )
        offset += kDocument()->lineLength( i ) + 1; // Add newline
    offset += cursor.column();
    return offset;
}

KTextEditor::Cursor KDocumentTextBuffer::offsetToCursor( unsigned int offset )
{
    int i;
    for( i = 0; offset > kDocument()->lineLength( i ); i++ )
        offset -= kDocument()->lineLength( i ) + 1; // Subtract newline
    return KTextEditor::Cursor( i, offset );
}

/* Accepting the session and buffer as parameters, although we
   could obtain them from the session proxy, ensures some type
   safety. */
InfTextDocument::InfTextDocument( QInfinity::SessionProxy &proxy,
    QInfinity::TextSession &session,
    KDocumentTextBuffer &buffer )
    : Document( *(buffer.kDocument()) )
    , m_sessionProxy( &proxy )
    , m_session( &session )
    , m_buffer( &buffer )
    , insert_count( 0 )
    , undo_count( 0 )
{
    setCollaborative( true );
    m_session->setParent( this );
    m_sessionProxy->setParent( this );
    connect( kDocument(), SIGNAL(viewCreated(KTextEditor::Document*, KTextEditor::View*)),
        this, SLOT(slotViewCreated(KTextEditor::Document*, KTextEditor::View*)) );
    synchronize();
}

InfTextDocument::~InfTextDocument()
{
    m_session->close();
}

void InfTextDocument::undo()
{
    if( m_user )
        m_session->undo( *m_user );
}

void InfTextDocument::redo()
{
    if( m_user )
        m_session->redo( *m_user );
}

void InfTextDocument::slotSynchronized()
{
    if( m_session->status() == QInfinity::Session::Running )
    {
        setLoadState( Document::SynchronizationComplete );
        joinSession();
    }
    else
    {
        throwFatalError( i18n("Synchronization ended but session is not running.") );
    }
}

void InfTextDocument::slotSynchronizationFailed( GError *gerror )
{
    QString emsg = i18n( "Synchronization Failed: " );
    emsg.append( gerror->message );
    throwFatalError( emsg );
}

void InfTextDocument::slotJoinFinished( QPointer<QInfinity::User> user )
{
    m_buffer->setUser( user );
    m_user = dynamic_cast<QInfinity::AdoptedUser*>(user.data());
    setLoadState( Document::JoiningComplete );
    setLoadState( Document::Complete );
    kDebug() << "Join successful.";
}

void InfTextDocument::slotJoinFailed( GError *gerror )
{
    QString emsg = i18n( "Could not join session: " );
    emsg.append( gerror->message );
    throwFatalError( emsg );
    kDebug() << "Join failed: " << emsg;
}

void InfTextDocument::slotViewCreated( KTextEditor::Document *kDoc,
    KTextEditor::View *kView )
{
    Q_UNUSED(kDoc)
    // HACK: Steal the undo/redo actions
    QAction *act = kView->action( "edit_undo" );
    if( act )
    {
        undo_actions.append( act );
        act->disconnect();
        connect( act, SIGNAL(triggered(bool)),
            this, SLOT(undo()) );
    }
    act = kView->action( "edit_redo" );
    if( act )
    {
        redo_actions.append( act );
        act->disconnect();
        connect( act, SIGNAL(triggered(bool)),
            this, SLOT(redo()) );
    }
}

void InfTextDocument::synchronize()
{
    if( m_session->status() == QInfinity::Session::Running )
        slotSynchronized();
    else if( m_session->status() == QInfinity::Session::Synchronizing )
    {
        setLoadState( Document::Synchronizing );
        connect( m_session, SIGNAL(synchronizationComplete()),
            this, SLOT(slotSynchronized()) );
        connect( m_session, SIGNAL(synchronizationFailed( GError* )),
            this, SLOT(slotSynchronizationFailed( GError* )) );
    }
}

void InfTextDocument::joinSession()
{
    setLoadState( Document::Joining );
    QInfinity::UserRequest *req = QInfinity::TextSession::joinUser( m_sessionProxy,
        KobbySettings::nickName(),
        10 );
    connect( req, SIGNAL(finished(QPointer<QInfinity::User>)),
        this, SLOT(slotJoinFinished(QPointer<QInfinity::User>)) );
    connect( req, SIGNAL(failed(GError*)),
        this, SLOT(slotJoinFailed(GError*)) );
}

}

