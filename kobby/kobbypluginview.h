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

#ifndef KOBBYPLUGINVIEW_H
#define KOBBYPLUGINVIEW_H

#include <QObject>
#include <QLabel>
#include <KTextEditor/View>

#include <libqinfinity/xmlconnection.h>

#include "common/document.h"

namespace Kobby {
    class Connection;
}
using Kobby::Connection;
using Kobby::Document;

class ManagedDocument;

namespace QInfinity {
    class User;
}

class KobbyPluginView;

class KobbyStatusBar : public QWidget {
Q_OBJECT
public:
    explicit KobbyStatusBar(KobbyPluginView* parent, Qt::WindowFlags f = 0);

public slots:
    void connectionStatusChanged(Connection*, QInfinity::XmlConnection::Status status);
    void sessionFullyReady();
    void usersChanged();

private:
    QLabel* m_connectionStatusLabel;
    QLabel* m_usersLabel;
    KobbyPluginView* m_view;
};

class KobbyPluginView : public QObject
{
Q_OBJECT
public:
    KobbyPluginView(KTextEditor::View* kteView, ManagedDocument* document);
    virtual ~KobbyPluginView();
    KobbyStatusBar* statusBar() const;

public slots:
    void remoteTextChanged(const KTextEditor::Range range, QInfinity::User* user);
    void documentReady(ManagedDocument*);

private:
    KTextEditor::View* m_view;
    KobbyStatusBar* m_statusBar;
    ManagedDocument* m_document;

    friend class KobbyStatusBar;
};

#endif // KOBBYPLUGINVIEW_H
