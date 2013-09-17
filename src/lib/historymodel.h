/****************************************************************************
 *   Copyright (C) 2012-2013 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H
//Base
#include "typedefs.h"
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QStringList>

//Qt

//SFLPhone
#include "call.h"

//Typedef
typedef QMap<uint, Call*>  CallMap;
typedef QList<Call*>          CallList;

///HistoryModel: History call manager
class LIB_EXPORT HistoryModel : public QAbstractItemModel {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:

   //Singleton
   static HistoryModel* instance();

   //Getters
   static const CallMap&    getHistory             ();
   static const QStringList getHistoryCallId       ();
   int acceptedPayloadTypes();

   //Setters
   void setCategoryRole(Call::Role role);

   //Mutator
   void add(Call* call);

   //Model implementation
   virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role   );
   virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole        ) const;
   virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                   ) const;
   virtual Qt::ItemFlags flags       ( const QModelIndex& index                                    ) const;
   virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                   ) const __attribute__ ((const));
   virtual QModelIndex   parent      ( const QModelIndex& index                                    ) const;
   virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent=QModelIndex()) const;
   virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
   virtual QStringList   mimeTypes   (                                                             ) const;
   virtual QMimeData*    mimeData    ( const QModelIndexList &indexes                              ) const;
   virtual bool dropMimeData         ( const QMimeData*, Qt::DropAction, int, int, const QModelIndex& );


private:

   //Constructor
   explicit HistoryModel();
   ~HistoryModel();

   bool initHistory ();

   //Static attributes
   static HistoryModel* m_spInstance;

   //Attributes
   static CallMap m_sHistoryCalls;
   bool m_HistoryInit;

   //Model
   class TopLevelItem : public CategorizedCompositeNode,public QObject {
   friend class HistoryModel;
   public:
      virtual QObject* getSelf();
      virtual ~TopLevelItem();
   private:
      explicit TopLevelItem(int name);
      CallList m_lChildren;
      int m_Name; //HistoryConst
      QString m_NameStr;
   };

   //Model categories
   QList<TopLevelItem*>         m_lCategoryCounter ;
   QHash<int,TopLevelItem*>     m_hCategories      ;
   bool                         m_isContactDateInit;
   int                          m_Role             ;
   bool                         m_ShowAll          ;
   bool                         m_HaveContactModel ;
   QStringList                  m_lMimes           ;

private Q_SLOTS:
   void reloadCategories();

Q_SIGNALS:
   ///Emitted when the history change (new items, cleared)
   void historyChanged          (            );
   ///Emitted when a new item is added to prevent full reload
   void newHistoryCall          ( Call* call );
};

#endif
