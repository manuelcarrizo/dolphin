/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "konq_listview.h"
#include "konq_listviewitems.h"
#include "konq_listviewwidget.h"
#include "konq_textviewwidget.h"
#include "konq_treeviewwidget.h"

#include <kcursor.h>
#include <konqdirlister.h>
#include <konqfileitem.h>
#include <kio/paste.h>
#include <kio/job.h>
#include <kdebug.h>
#include <konq_propsview.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kparts/mainwindow.h>
#include <kparts/partmanager.h>
#include <klineeditdlg.h>
#include <kpropsdlg.h>
#include <kprotocolmanager.h>

#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <qkeycode.h>
#include <qlist.h>
#include <qdragobject.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qstringlist.h>
#include <klocale.h>
#include <klibloader.h>
#include <qregexp.h>

KonqListViewFactory::KonqListViewFactory()
{
  s_instance = 0;
  s_defaultViewProps = 0;
}

KonqListViewFactory::~KonqListViewFactory()
{
  if ( s_instance )
    delete s_instance;
  if ( s_defaultViewProps )
    delete s_defaultViewProps;

  s_instance = 0;
  s_defaultViewProps = 0;
}

KParts::Part* KonqListViewFactory::createPart( QWidget *parentWidget, const char *, QObject *parent, const char *name, const char*, const QStringList &args )
{
  if( args.count() < 1 )
    kdWarning() << "KonqListView: Missing Parameter" << endl;

  KParts::Part *obj = new KonqListView( parentWidget, parent, name, args.first() );
  emit objectCreated( obj );
  return obj;
}

KInstance *KonqListViewFactory::instance()
{
  if ( !s_instance )
    s_instance = new KInstance( "konqlistview" );
  return s_instance;
}

KonqPropsView *KonqListViewFactory::defaultViewProps()
{
  if ( !s_defaultViewProps )
    s_defaultViewProps = new KonqPropsView( instance(),0L );
    //s_defaultViewProps = KonqPropsView::defaultProps( instance() );

  return s_defaultViewProps;
}

KInstance *KonqListViewFactory::s_instance = 0;
KonqPropsView *KonqListViewFactory::s_defaultViewProps = 0;

extern "C"
{
  void *init_libkonqlistview()
  {
    return new KonqListViewFactory;
  }
};

ListViewBrowserExtension::ListViewBrowserExtension( KonqListView *listView )
 : KParts::BrowserExtension( listView )
{
  m_listView = listView;
}

int ListViewBrowserExtension::xOffset()
{
  return m_listView->listViewWidget()->contentsX();
}

int ListViewBrowserExtension::yOffset()
{
  return m_listView->listViewWidget()->contentsY();
}

void ListViewBrowserExtension::updateActions()
{
  // This code is very related to KonqIconViewWidget::slotSelectionChanged

  QValueList<KonqBaseListViewItem*> selection;
  m_listView->listViewWidget()->selectedItems( selection );

  bool cutcopy, del;
  bool bInTrash = false;
  QValueList<KonqBaseListViewItem*>::ConstIterator it = selection.begin();
  KFileItem * firstSelectedItem = 0L;
  for (; it != selection.end(); ++it )
  {
    if ( (*it)->item()->url().directory(false) == KGlobalSettings::trashPath() )
      bInTrash = true;
    if ( ! firstSelectedItem )
        firstSelectedItem = (*it)->item();
  }
  cutcopy = del = ( selection.count() > 0 );

  emit enableAction( "copy", cutcopy );
  emit enableAction( "cut", cutcopy );
  emit enableAction( "trash", del && !bInTrash );
  emit enableAction( "del", del );
  emit enableAction( "shred", del );

  bool bKIOClipboard = !KIO::isClipboardEmpty();
  QMimeSource *data = QApplication::clipboard()->data();
  bool paste = ( bKIOClipboard || data->encodedData( data->format() ).size() != 0 ) &&
    (selection.count() == 1); // Let's allow pasting only on an item, not on the background

  emit enableAction( "pastecut", paste );
  emit enableAction( "pastecopy", paste );

  KFileItemList lstItems;
  if ( firstSelectedItem )
      lstItems.append( firstSelectedItem );
  emit enableAction( "properties", ( selection.count() == 1 ) &&
                     KPropertiesDialog::canDisplay( lstItems ) );
  emit enableAction( "editMimeType", ( selection.count() == 1 ) );
}

void ListViewBrowserExtension::cut()
{
  //TODO: grey out item
  copy();
}

void ListViewBrowserExtension::copy()
{
  QValueList<KonqBaseListViewItem*> selection;

  m_listView->listViewWidget()->selectedItems( selection );

  QStringList lstURLs;

  QValueList<KonqBaseListViewItem*>::ConstIterator it = selection.begin();
  QValueList<KonqBaseListViewItem*>::ConstIterator end = selection.end();
  for (; it != end; ++it )
    lstURLs.append( (*it)->item()->url().url() );

  QUriDrag *urlData = new QUriDrag;
  urlData->setUnicodeUris( lstURLs );
  QApplication::clipboard()->setData( urlData );
}

void ListViewBrowserExtension::pasteSelection( bool move )
{
  QValueList<KonqBaseListViewItem*> selection;
  m_listView->listViewWidget()->selectedItems( selection );
  assert ( selection.count() == 1 );
  KIO::pasteClipboard( selection.first()->item()->url(), move );
}

void ListViewBrowserExtension::reparseConfiguration()
{
  // m_pProps is a problem here (what is local, what is global ?)
  // but settings is easy :
  m_listView->listViewWidget()->initConfig();
}

void ListViewBrowserExtension::setSaveViewPropertiesLocally(bool value)
{
   m_listView->listViewWidget()->m_pProps->setSaveViewPropertiesLocally( value );
};
/*void ListViewBrowserExtension::saveLocalProperties()
{
  // TODO move this to KonqListView. Ugly.
  m_listView->listViewWidget()->m_pProps->saveLocal( m_listView->url() );
}*/

/*void ListViewBrowserExtension::savePropertiesAsDefault()
{
  m_listView->listViewWidget()->m_pProps->saveAsDefault( KonqListViewFactory::instance() );
}*/

void ListViewBrowserExtension::properties()
{
    QValueList<KonqBaseListViewItem*> selection;
    m_listView->listViewWidget()->selectedItems( selection );
    assert ( selection.count() == 1 );
    (void) new KPropertiesDialog( selection.first()->item() );
}

void ListViewBrowserExtension::editMimeType()
{
    QValueList<KonqBaseListViewItem*> selection;
    m_listView->listViewWidget()->selectedItems( selection );
    assert ( selection.count() == 1 );
    KonqOperations::editMimeType( selection.first()->item()->mimetype() );
}

KonqListView::KonqListView( QWidget *parentWidget, QObject *parent, const char *name, const QString& mode )
 : KParts::ReadOnlyPart( parent, name )
{
  setInstance( KonqListViewFactory::instance() );

  m_browser = new ListViewBrowserExtension( this );

  if (mode=="TextView")
  {
     kdDebug(1202) << "Creating KonqTextViewWidget\n" << endl;
     setXMLFile( "konq_textview.rc" );
     m_pListView=new KonqTextViewWidget(this, parentWidget);
  }
  else if (mode=="MixedTree")
  {
     kdDebug(1202) << "Creating KonqTreeViewWidget\n" << endl;
     setXMLFile( "konq_treeview.rc" );
     m_pListView=new KonqTreeViewWidget(this,parentWidget);
  }
  else
  {
     kdDebug(1202) << "Creating KonqDetailedListViewWidget\n" << endl;
     setXMLFile( "konq_detailedlistview.rc" );
     m_pListView = new KonqBaseListViewWidget( this, parentWidget);
  };
  setWidget( m_pListView );

  setupActions();

   m_pListView->confColumns.append(new ColumnInfo(I18N_NOOP("Type"),"Type",KIO::UDS_FILE_TYPE,0,FALSE,m_paShowType));
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Size"),"Size",KIO::UDS_SIZE,0,FALSE,m_paShowSize) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Modified"),"Date",KIO::UDS_MODIFICATION_TIME,0,FALSE,m_paShowTime) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Accessed"),"AccessDate",KIO::UDS_ACCESS_TIME,0,FALSE,m_paShowAccessTime) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Created"),"CreationDate",KIO::UDS_CREATION_TIME,0,FALSE,m_paShowCreateTime) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Permissions"),"Access",KIO::UDS_ACCESS,0,FALSE,m_paShowPermissions) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Owner"),"Owner",KIO::UDS_USER,0,FALSE,m_paShowOwner) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Group"),"Group",KIO::UDS_GROUP,0,FALSE,m_paShowGroup) );
   m_pListView->confColumns.append( new ColumnInfo(I18N_NOOP("Link"),"Link",KIO::UDS_LINK_DEST,0,FALSE,m_paShowLinkDest) );
/*   tmpCount++;
   tmpInfo=new ColumnInfo(I18N_NOOP("URL"),KIO::UDS_URL,tmpCount,FALSE,someAction);
   confColumns.append( tmpInfo );
   confColumnsPos.insert(tmpInfo->name(),new int(tmpCount));
   tmpCount++;
   tmpInfo=new ColumnInfo(I18N_NOOP("MimeType"),KIO::UDS_MIME_TYPE,tmpCount,FALSE);
   confColumns.append( tmpInfo );
   confColumnsPos.insert(tmpInfo->name(),new int(tmpCount));*/

  QObject::connect( m_pListView, SIGNAL( selectionChanged() ),
                    m_browser, SLOT( updateActions() ) );
}

KonqListView::~KonqListView()
{}

bool KonqListView::openURL( const KURL &url )
{
  m_url = url;

  KURL u( url );

  emit setWindowCaption( u.decodedURL() );

  return m_pListView->openURL( url );
}

bool KonqListView::closeURL()
{
  m_pListView->stop();
  return true;
}

void KonqListView::guiActivateEvent( KParts::GUIActivateEvent *event )
{
  KParts::ReadOnlyPart::guiActivateEvent( event );
  if ( event->activated() )
    m_browser->updateActions();
}


void KonqListView::slotSelect()
{
   KLineEditDlg l( i18n("Select files:"), "*", m_pListView );
   if ( l.exec() )
   {
      QString pattern = l.text();
      if ( pattern.isEmpty() )
         return;

      QRegExp re( pattern, true, true );

      m_pListView->blockSignals( true );

      for (KonqBaseListViewWidget::iterator it = m_pListView->begin(); it != m_pListView->end(); it++ )
         if ( re.match( it->text(m_pListView->m_filenameColumn) ) != -1 )
            it->setSelected( TRUE);

      m_pListView->blockSignals( false );
      m_pListView->repaintContents(0,0,m_pListView->width(),m_pListView->height());
      //m_pListView->updateSelectedFilesInfo();

      // do this once, not for each item
      //m_pListView.slotSelectionChanged();
      //slotDisplayFileSelectionInfo();
   }
   emit m_pListView->selectionChanged();
   m_pListView->updateSelectedFilesInfo();
}

void KonqListView::slotUnselect()
{
   KLineEditDlg l( i18n("Unselect files:"), "*", m_pListView );
   if ( l.exec() )
   {
      QString pattern = l.text();
      if ( pattern.isEmpty() )
         return;

      QRegExp re( pattern, TRUE, TRUE );

      m_pListView->blockSignals(TRUE);

      for (KonqBaseListViewWidget::iterator it = m_pListView->begin(); it != m_pListView->end(); it++ )
         if ( re.match( it->text(m_pListView->m_filenameColumn) ) != -1 )
            it->setSelected(FALSE);

      m_pListView->blockSignals(FALSE);
      m_pListView->repaintContents(0,0,m_pListView->width(),m_pListView->height());

      // do this once, not for each item
      //m_pListView.slotSelectionChanged();
      //slotDisplayFileSelectionInfo();
      //m_pListView->updateSelectedFilesInfo();
   }
   emit m_pListView->selectionChanged();
   m_pListView->updateSelectedFilesInfo();
}

void KonqListView::slotSelectAll()
{
   m_pListView->selectAll(TRUE);
   //m_pListView->updateSelectedFilesInfo();
   emit m_pListView->selectionChanged();
   m_pListView->updateSelectedFilesInfo();
}

void KonqListView::slotUnselectAll()
{
    m_pListView->selectAll(FALSE);
    //m_pListView->updateSelectedFilesInfo();
   emit m_pListView->selectionChanged();
   m_pListView->updateSelectedFilesInfo();
}


void KonqListView::slotInvertSelection()
{
    m_pListView->invertSelection();
    //m_pListView->updateSelectedFilesInfo();
    m_pListView->repaintContents(0,0,m_pListView->width(),m_pListView->height());
    emit m_pListView->selectionChanged();
    m_pListView->updateSelectedFilesInfo();
}

void KonqListView::slotIconSizeToggled( bool)
{
   if (m_paLargeIcons->isChecked()) m_pListView->m_iconSize=KIcon::SizeLarge;
   else if (m_paMediumIcons->isChecked()) m_pListView->m_iconSize=KIcon::SizeMedium;
   else m_pListView->m_iconSize=KIcon::SizeSmall;
   m_pListView->updateListContents();
}

void KonqListView::slotShowDot()
{
  m_pListView->dirLister()->setShowingDotFiles( m_paShowDot->isChecked() );
}
void KonqListView::slotColumnToggled(bool)
{
   kDebugInfo(1202,"::slotColumnToggled\n");
   for (int i=0; i<m_pListView->confColumns.count(); i++)
      m_pListView->confColumns.at(i)->displayThisOne=m_pListView->confColumns.at(i)->toggleThisOne->isChecked()&&m_pListView->confColumns.at(i)->toggleThisOne->isEnabled();

   //create the new list contents
   m_pListView->createColumns();
   m_pListView->updateListContents();

   //save the config
   KConfig * config = KGlobal::config();
   QString groupName="ListView_" + m_pListView->url().protocol();
   config->setGroup( groupName );
   QStringList lstColumns;
   for (int i=0; i<m_pListView->confColumns.count(); i++)
   {
      if (m_pListView->confColumns.at(i)->displayThisOne) lstColumns.append(m_pListView->confColumns.at(i)->name);
   };
   config->writeEntry("Columns",lstColumns);
   config->sync();
};

void KonqListView::slotBackgroundColor()
{
  //TODO
}

void KonqListView::slotBackgroundImage()
{
  //TODO
}

void KonqListView::slotReloadTree()
{
//  m_pListView->openURL( url(), m_pListView->contentsX(), m_pListView->contentsY() );
}

void KonqListView::setupActions()
{
   m_paShowTime=new KToggleAction(i18n("Show &Modification Time"), 0,this, SLOT(slotColumnToggled(bool)), actionCollection(), "show_time" );
   m_paShowType=new KToggleAction(i18n("Show &File Type"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_type" );
   m_paShowAccessTime=new KToggleAction(i18n("Show &Access Time"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_access_time" );
   m_paShowCreateTime=new KToggleAction(i18n("Show &Creation Time"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_creation_time" );
   m_paShowLinkDest=new KToggleAction(i18n("Show &Link Destination"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_link_dest" );
   m_paShowSize=new KToggleAction(i18n("Show Filesize"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_size" );
   m_paShowOwner=new KToggleAction(i18n("Show Owner"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_owner" );
   m_paShowGroup=new KToggleAction(i18n("Show Group"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_group" );
   m_paShowPermissions=new KToggleAction(i18n("Show permissions"), 0, this, SLOT(slotColumnToggled(bool)),actionCollection(), "show_permissions" );

   m_paSelect = new KAction( i18n( "&Select..." ), CTRL+Key_Plus, this, SLOT( slotSelect() ), actionCollection(), "select" );
  m_paUnselect = new KAction( i18n( "&Unselect..." ), CTRL+Key_Minus, this, SLOT( slotUnselect() ), actionCollection(), "unselect" );
  m_paSelectAll = KStdAction::selectAll( this, SLOT( slotSelectAll() ), this, "selectall" );
  m_paUnselectAll = new KAction( i18n( "U&nselect All" ), CTRL+Key_U, this, SLOT( slotUnselectAll() ), actionCollection(), "unselectall" );
  m_paInvertSelection = new KAction( i18n( "&Invert Selection" ), CTRL+Key_Asterisk, this, SLOT( slotInvertSelection() ), actionCollection(), "invertselection" );

  m_paLargeIcons = new KToggleAction( i18n( "&Large" ), 0, actionCollection(), "modelarge" );
  m_paMediumIcons = new KToggleAction( i18n( "&Medium" ), 0, actionCollection(), "modemedium" );
  m_paSmallIcons = new KToggleAction( i18n( "&Small" ), 0, actionCollection(), "modesmall" );
//  m_paNoIcons = new KToggleAction( i18n( "&Disabled" ), 0, actionCollection(), "modenone" );

  m_paShowDot = new KToggleAction( i18n( "Show &Dot Files" ), 0, this, SLOT( slotShowDot() ), actionCollection(), "show_dot" );
//  m_paCheckMimeTypes = new KToggleAction( i18n( "Determine &File Types" ), 0, this, SLOT( slotCheckMimeTypes() ), actionCollection(), "ckeck_mimetypes" );
  /*KAction * m_paBackgroundColor =*/ new KAction( i18n( "Background Color..." ), 0, this, SLOT( slotBackgroundColor() ), actionCollection(), "bgcolor" );
  /*KAction * m_paBackgroundImage =*/ new KAction( i18n( "Background Image..." ), 0, this, SLOT( slotBackgroundImage() ), actionCollection(), "bgimage" );

  m_paLargeIcons->setExclusiveGroup( "ViewMode" );
  m_paMediumIcons->setExclusiveGroup( "ViewMode" );
  m_paSmallIcons->setExclusiveGroup( "ViewMode" );
  //m_paNoIcons->setExclusiveGroup( "ViewMode" );

  m_paLargeIcons->setChecked( false );
  m_paMediumIcons->setChecked( false );
  m_paSmallIcons->setChecked( true );
//  m_paNoIcons->setChecked( false );

//  m_paCheckMimeTypes->setChecked( true );

  connect( m_paLargeIcons, SIGNAL( toggled( bool ) ), this, SLOT( slotIconSizeToggled( bool ) ) );
  connect( m_paMediumIcons, SIGNAL( toggled( bool ) ), this, SLOT( slotIconSizeToggled( bool ) ) );
  connect( m_paSmallIcons, SIGNAL( toggled( bool ) ), this, SLOT( slotIconSizeToggled( bool ) ) );
//  connect( m_paNoIcons, SIGNAL( toggled( bool ) ), this, SLOT( slotViewNone( bool ) ) );
}

#include "konq_listview.moc"


