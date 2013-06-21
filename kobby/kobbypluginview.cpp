/* This file is part of the Kobby
 * Copyright (C) 2013 Sven Brauch <svenbrauch@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "kobbypluginview.h"
#include "manageddocument.h"
#include "kobbyplugin.h"
#include "common/ui/remotechangenotifier.h"

#include <libqinfinity/user.h>
#include <libqinfinity/usertable.h>

#include <QLayout>
#include <QLabel>
#include <QAction>
#include <QLineEdit>

#include <KLocalizedString>
#include <KActionCollection>
#include <KIcon>
#include <KAction>
#include <KDialog>
#include <KMessageBox>

KobbyStatusBar::KobbyStatusBar(KobbyPluginView* parent, Qt::WindowFlags f)
    : QWidget(parent->m_view, f)
    , m_connectionStatusLabel(new QLabel(this))
    , m_view(parent)
    , m_usersLabel(new QLabel(this))
{
    setLayout(new QHBoxLayout());
    layout()->setAlignment(Qt::AlignRight);
    layout()->addWidget(m_connectionStatusLabel);
    layout()->addWidget(m_usersLabel);
}

void KobbyStatusBar::connectionStatusChanged(Kobby::Connection*, QInfinity::XmlConnection::Status status)
{
    QString text;
    if ( status == QInfinity::XmlConnection::Closed ) {
        // Since the connection will start opening immediately,
        // this will not display in the beginning, just on disconnect
        text =   "<b><span style=\"color:#C30000\">"
               + i18n("Disconnected from collaboration server.")
               + "</span></b>";
    }
    else if ( status == QInfinity::XmlConnection::Opening ) {
        text = i18n("Connecting...");
    }
    else if ( status == QInfinity::XmlConnection::Open ) {
        text = i18n("Synchronizing document...");
    }
    else if ( status == QInfinity::XmlConnection::Closing ) {
        text = i18n("Disconnecting...");
    }
    m_connectionStatusLabel->setText(text);
}

void KobbyStatusBar::sessionFullyReady()
{
    m_connectionStatusLabel->setText(  "<b><span style=\"color:#009D0A\">"
                                     + i18n("Connected to collaboration server.")
                                     + "</span></b>" );
}

void KobbyStatusBar::usersChanged()
{
    QList< QPointer< QInfinity::User > > users = m_view->m_document->userTable()->users();
    foreach ( const QPointer<QInfinity::User>& user, users ) {
        connect(user.data(), SIGNAL(statusChanged()), this, SLOT(usersChanged()), Qt::UniqueConnection);
    }
    QList< QPointer< QInfinity::User > > activeUsers = m_view->m_document->userTable()->activeUsers();
    m_usersLabel->setText(i18np("%1 active user in session", "%1 active users in session", activeUsers.size()));
}

KobbyStatusBar* KobbyPluginView::statusBar() const
{
    return m_statusBar;
}

KobbyPluginView::KobbyPluginView(KTextEditor::View* kteView, ManagedDocument* document)
    : QObject(kteView)
    , KXMLGUIClient(kteView)
    , m_view(kteView)
    , m_statusBar(0)
    , m_document(document)
{
    if ( m_document ) {
        // See KobbyPlugin::addView
        documentBecameManaged(document);
    }

    setComponentData(KobbyPluginFactory::componentData());
    setXMLFile("ktexteditor_kobbyui.rc");

    // Set up the actions for the "Collaborative" menu
    m_openCollabDocumentAction = actionCollection()->addAction("kobby_open", this, SLOT(openActionClicked()));
    m_openCollabDocumentAction->setText(i18n("Open collaborative document..."));
    m_openCollabDocumentAction->setToolTip(i18n("Open a collaborative document from a manually specified server"));

    m_saveCopyAction = actionCollection()->addAction("kobby_save_copy", this, SLOT(saveCopyActionClicked()));
    m_saveCopyAction->setText(i18n("Save a local copy..."));
    m_saveCopyAction->setHelpText(i18n("Save a local copy of the current document, but continue "
                                      "synchronizing changes"));

    m_shareWithContactAction = actionCollection()->addAction("kobby_share_with_contact", this, SLOT(shareActionClicked()));
    m_shareWithContactAction->setText(i18n("Share document..."));
    m_shareWithContactAction->setHelpText(i18n("Collaboratively edit the current document with an "
                                              "instant messenger contact"));

    m_changeUserNameAction = actionCollection()->addAction("kobby_change_user_name", this, SLOT(changeUserActionClicked()));
    m_changeUserNameAction->setText(i18n("Change user name..."));
    m_changeUserNameAction->setHelpText(i18n("Change your user name for the current document"));

    m_disconnectAction = actionCollection()->addAction("kobby_disconnect", this, SLOT(disconnectActionClicked()));
    m_disconnectAction->setText(i18n("Disconnect"));
    m_disconnectAction->setHelpText(i18n("Disconnect from the collaborative server, and stop"
                                        "synchronizing changes to the document"));
    m_disconnectAction->setEnabled(false);

    m_createServerAction = actionCollection()->addAction("kobby_create_server", this, SLOT(createServerActionClicked()));
    m_createServerAction->setText(i18n("Start collaborative server..."));
    m_createServerAction->setHelpText(i18n("Host a collaboration session. Other persons can join the session "
                                          "by using the \"Open collaborative document\" action."));
    m_createServerAction->setEnabled(false);
}

void KobbyPluginView::documentBecameManaged(ManagedDocument* document)
{
    if ( document->document() != m_view->document() ) {
        return;
    }
    m_document = document;
    enableUi();
}

void KobbyPluginView::documentBecameUnmanaged(ManagedDocument* document)
{
    if ( document != m_document || m_document == 0 ) {
        return;
    }
    Q_ASSERT(document->document() == m_document->document());
    m_document = 0;
    disableUi();
}

void KobbyPluginView::enableUi()
{
    m_statusBar = new KobbyStatusBar(this);
    connect(m_document->connection(), SIGNAL(statusChanged(Connection*,QInfinity::XmlConnection::Status)),
            statusBar(), SLOT(connectionStatusChanged(Connection*,QInfinity::XmlConnection::Status)), Qt::UniqueConnection);
    statusBar()->connectionStatusChanged(m_document->connection(), m_document->connection()->status());
    connect(m_document, SIGNAL(documentReady(ManagedDocument*)),
            this, SLOT(documentReady(ManagedDocument*)), Qt::UniqueConnection);
    m_view->layout()->addWidget(m_statusBar);
}

void KobbyPluginView::disableUi()
{
    m_view->layout()->removeWidget(m_statusBar);
    delete m_statusBar;
    m_statusBar = 0;
    // Connections are disconnected automatically since m_document will be deleted
}

void KobbyPluginView::disconnectActionClicked()
{
    kDebug();
}

void KobbyPluginView::changeUserActionClicked()
{
}

void KobbyPluginView::changeUserName()
{
}

void KobbyPluginView::createServerActionClicked()
{

}

void KobbyPluginView::openActionClicked()
{

}

void KobbyPluginView::saveCopyActionClicked()
{

}

void KobbyPluginView::shareActionClicked()
{

}

KobbyPluginView::~KobbyPluginView()
{
}

void KobbyPluginView::remoteTextChanged(const KTextEditor::Range range, QInfinity::User* user, bool removal)
{
    RemoteChangeNotifier::addNotificationWidget(m_view, range.start(), user->name());
}

void KobbyPluginView::documentReady(ManagedDocument* doc)
{
    connect(doc->textBuffer(), SIGNAL(remoteChangedText(KTextEditor::Range,QInfinity::User*,bool)),
            this, SLOT(remoteTextChanged(KTextEditor::Range,QInfinity::User*,bool)));
    connect(m_document->userTable(), SIGNAL(userAdded(User*)),
            statusBar(), SLOT(usersChanged()));
    connect(m_document->userTable(), SIGNAL(userRemoved(User*)),
            statusBar(), SLOT(usersChanged()));
    m_statusBar->usersChanged();
    statusBar()->sessionFullyReady();
    m_disconnectAction->setEnabled(true);
}

#include "kobbypluginview.moc"
