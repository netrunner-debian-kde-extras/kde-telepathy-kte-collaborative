/*
 * Copyright 2013 Sven Brauch <svenbrauch@gmail.com>
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

#include "infinotenotifier.h"

#include "kpluginfactory.h"

#include <QDebug>
#include <kdirnotify.h>

K_PLUGIN_FACTORY(InfinoteNotifierFactory, registerPlugin<InfinoteNotifier>();)
K_EXPORT_PLUGIN(InfinoteNotifierFactory("infinotenotifier"))

InfinoteNotifier::InfinoteNotifier(QObject* parent, const QVariantList& )
    : KDEDModule(parent)
    , m_notifyIface(new OrgKdeKDirNotifyInterface(QString(), QString(), QDBusConnection::sessionBus(), this))
{
    qDebug() << "Loaded module";
    qDebug() << connect(m_notifyIface, SIGNAL(enteredDirectory(QString)), SLOT(enteredDirectory(QString)));
    qDebug() << connect(m_notifyIface, SIGNAL(leftDirectory(QString)), SLOT(leftDirectory(QString)));
}

void InfinoteNotifier::enteredDirectory(QString path)
{
    if ( ! path.startsWith("inf") ) {
        return;
    }
    qDebug() << "entered directory" << path;
}

void InfinoteNotifier::leftDirectory(QString path)
{
    if ( ! path.startsWith("inf") ) {
        return;
    }
    qDebug() << "left directory" << path;
}

#include "infinotenotifier.moc"
