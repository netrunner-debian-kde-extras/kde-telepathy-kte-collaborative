#include <libinfinitymm/common/xmppconnection.h>
#include <libinfinitymm/common/connectionmanager.h>
#include <libinfinitymm/client/clientbrowser.h>
#include <libinfinitymm/client/clientbrowseriter.h>
#include <libinfinitymm/client/clientexplorerequest.h>

#include "filebrowser.h"

#include <qinfinitymm/qtio.h>

#include <kobby/infinote/connection.h>
#include <kobby/infinote/infinotemanager.h>

#include <KDebug>
#include <KIcon>
#include <KPushButton>

#include <glib/gerror.h>

namespace Kobby
{

FileBrowserWidgetItem::FileBrowserWidgetItem( QString name, const Infinity::ClientBrowserIter &iter, int type, QTreeWidget *parent )
    : QTreeWidgetItem( parent, type )
    , node( new Infinity::ClientBrowserIter() )
{
    *node = iter;
    setItemIcon();

    setText( 0, name );
}

FileBrowserWidgetItem::FileBrowserWidgetItem( const Infinity::ClientBrowserIter &iter, int type, QTreeWidget *parent )
    : QTreeWidgetItem( parent, type )
    , node( new Infinity::ClientBrowserIter() )
{
    *node = iter;
    setItemIcon();

    setText( 0, node->getName() );
}

FileBrowserWidgetItem::~FileBrowserWidgetItem()
{
    delete node;
}

void FileBrowserWidgetItem::setItemIcon()
{
    if( type() == Folder )
        setIcon( 0, KIcon( "folder.png" ) );
    else if( type() == Note )
        setIcon( 0, KIcon( "text-plain.png" ) );
}

FileBrowserWidgetFolderItem::FileBrowserWidgetFolderItem( Infinity::ClientBrowserIter &iter, QTreeWidget *parent )
    : FileBrowserWidgetItem( iter, FileBrowserWidgetItem::Folder, parent )
    , exploreRequest( new Glib::RefPtr<Infinity::ClientExploreRequest> )
{
    setupUi();
}

FileBrowserWidgetFolderItem::FileBrowserWidgetFolderItem( QString name, Infinity::ClientBrowserIter &iter, QTreeWidget *parent )
    : FileBrowserWidgetItem( name, iter, FileBrowserWidgetItem::Folder, parent )
    , exploreRequest( new Glib::RefPtr<Infinity::ClientExploreRequest> )
{
    setupUi();
}

FileBrowserWidgetFolderItem::~FileBrowserWidgetFolderItem()
{
    delete exploreRequest;
}

void FileBrowserWidgetFolderItem::populate( bool expand_when_finished )
{
    kDebug() << "Exploring";
    *exploreRequest = node->explore();
    (*exploreRequest)->signal_finished().connect( sigc::mem_fun( this, &FileBrowserWidgetFolderItem::exploreFinishedCb ) );
    (*exploreRequest)->signal_failed().connect( sigc::mem_fun( this, &FileBrowserWidgetFolderItem::exploreFailedCb ) );
}

void FileBrowserWidgetFolderItem::setupUi()
{
    setExpanded( false );
}

void FileBrowserWidgetFolderItem::exploreFinishedCb()
{
    kDebug() << "finished.";

    bool res;

    for( res = node->child(); res; res = node->next() )
    {
        addChild( new FileBrowserWidgetItem( node->getName(), *node, type() ) );
    }
}

void FileBrowserWidgetFolderItem::exploreFailedCb( GError *value )
{
    kDebug() << "failed.";
}

FileBrowserWidget::FileBrowserWidget( const Connection &conn, QWidget *parent )
    : QTreeWidget( parent )
    , infinoteManager( &conn.getInfinoteManager() )
    , clientBrowser(  &conn.getClientBrowser() )
    , connection( &conn )
    , rootNode( 0 )
{
    setupUi();
    createRootNodes();
}

FileBrowserWidget::~FileBrowserWidget()
{
    delete rootNode;
}

void FileBrowserWidget::setupUi()
{
    setHeaderLabel( "Nodes" );
}

void FileBrowserWidget::createRootNodes()
{
    FileBrowserWidgetFolderItem *rootNodeItem;

    rootNode = new Infinity::ClientBrowserIter();
    clientBrowser->setRootNode( *rootNode );
    exploreRequest = new Glib::RefPtr<Infinity::ClientExploreRequest>();
    *exploreRequest = clientBrowser->explore( *rootNode );
    //rootNodeItem = new FileBrowserWidgetFolderItem( "/", *rootNode, this );
    //addTopLevelItem( rootNodeItem );
    //rootNodeItem->populate( true );
}

FileBrowserDialog::FileBrowserDialog( const Connection &conn, QWidget *parent )
    : KDialog( parent )
    , fileBrowserWidget( new FileBrowserWidget( conn, this ) )
{
    setupUi();
}

FileBrowserDialog::~FileBrowserDialog()
{
}

void FileBrowserDialog::setupUi()
{
    setCaption( "File Browser" );
    setMainWidget( fileBrowserWidget );
}

}