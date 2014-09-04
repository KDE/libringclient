/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
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
#include "phonedirectorymodel.h"

//Qt
#include <QtCore/QCoreApplication>

//SFLPhone
#include "phonenumber.h"
#include "call.h"
#include "uri.h"
#include "account.h"
#include "contact.h"
#include "accountlistmodel.h"
#include "numbercategory.h"
#include "numbercategorymodel.h"
#include "abstractitembackend.h"
#include "dbus/presencemanager.h"
#include "visitors/pixmapmanipulationvisitor.h"
#include "contactmodel.h"

PhoneDirectoryModel* PhoneDirectoryModel::m_spInstance = nullptr;

PhoneDirectoryModel::PhoneDirectoryModel(QObject* parent) :
   QAbstractTableModel(parent?parent:QCoreApplication::instance()),m_CallWithAccount(false)
{
   setObjectName("PhoneDirectoryModel");
   connect(&DBus::PresenceManager::instance(),SIGNAL(newBuddyNotification(QString,QString,bool,QString)),this,
           SLOT(slotNewBuddySubscription(QString,QString,bool,QString)));
}

PhoneDirectoryModel::~PhoneDirectoryModel()
{
   QList<NumberWrapper*> vals = m_hNumbersByNames.values();
   m_hNumbersByNames.clear();
   m_lSortedNames.clear();
   while (vals.size()) {
      NumberWrapper* w = vals[0];
      vals.removeAt(0);
      delete w;
   }
   vals = m_hSortedNumbers.values();
   m_hSortedNumbers.clear();
   m_hDirectory.clear();
   while (vals.size()) {
      NumberWrapper* w = vals[0];
      vals.removeAt(0);
      delete w;
   }
}

PhoneDirectoryModel* PhoneDirectoryModel::instance()
{
   if (!m_spInstance) {
      m_spInstance = new PhoneDirectoryModel();
   }
   return m_spInstance;
}

QVariant PhoneDirectoryModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid() || index.row() >= m_lNumbers.size()) return QVariant();
   const PhoneNumber* number = m_lNumbers[index.row()];
   switch (static_cast<PhoneDirectoryModel::Columns>(index.column())) {
      case PhoneDirectoryModel::Columns::URI:
         switch (role) {
            case Qt::DisplayRole:
               return number->uri();
               break;
            case Qt::DecorationRole :
               return PixmapManipulationVisitor::instance()->callPhoto(number,QSize(16,16));
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::TYPE:
         switch (role) {
            case Qt::DisplayRole:
               return number->category()->name();
               break;
            case Qt::DecorationRole:
               return number->icon();
         }
         break;
      case PhoneDirectoryModel::Columns::CONTACT:
         switch (role) {
            case Qt::DisplayRole:
               return number->contact()?number->contact()->formattedName():QVariant();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::ACCOUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->account()?number->account()->id():QVariant();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::STATE:
         switch (role) {
            case Qt::DisplayRole:
               return (int)number->type();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::CALL_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->callCount();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::LAST_USED:
         switch (role) {
            case Qt::DisplayRole:
               return (int)number->lastUsed();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::NAME_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->alternativeNames().size();
               break;
            case Qt::ToolTipRole: {
               QString out = "<table>";
               QHashIterator<QString, int> iter(number->alternativeNames());
               while (iter.hasNext())
                  out += QString("<tr><td>%1</td><td>%2</td></tr>").arg(iter.value()).arg(iter.key());
               out += "</table>";
               return out;
            }
         }
         break;
      case PhoneDirectoryModel::Columns::TOTAL_SECONDS:
         switch (role) {
            case Qt::DisplayRole:
               return number->totalSpentTime();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::WEEK_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->weekCount();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::TRIM_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->trimCount();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::HAVE_CALLED:
         switch (role) {
            case Qt::DisplayRole:
               return number->haveCalled();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::POPULARITY_INDEX:
         switch (role) {
            case Qt::DisplayRole:
               return (int)number->popularityIndex();
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::BOOKMARED:
         switch (role) {
            case Qt::CheckStateRole:
               return (bool)number->isBookmarked()?Qt::Checked:Qt::Unchecked;
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::TRACKED:
         switch (role) {
            case Qt::CheckStateRole:
               if (number->account() && number->account()->supportPresenceSubscribe())
                  return number->isTracked()?Qt::Checked:Qt::Unchecked;
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::PRESENT:
         switch (role) {
            case Qt::CheckStateRole:
               return number->isPresent()?Qt::Checked:Qt::Unchecked;
               break;
         }
         break;
      case PhoneDirectoryModel::Columns::PRESENCE_MESSAGE:
         switch (role) {
            case Qt::DisplayRole: {
               if ((index.column() == static_cast<int>(PhoneDirectoryModel::Columns::TRACKED)
                  || static_cast<int>(PhoneDirectoryModel::Columns::PRESENT))
                  && number->account() && (!number->account()->supportPresenceSubscribe())) {
                  return tr("This account does not support presence tracking");
               }
               else if (!number->account())
                  return tr("No associated account");
               else
                  return number->presenceMessage();
            } break;
         }
         break;
      case PhoneDirectoryModel::Columns::UID:
         switch (role) {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
               return number->uid();
               break;
         }
         break;
   }
   return QVariant();
}

int PhoneDirectoryModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;
   return m_lNumbers.size();
}

int PhoneDirectoryModel::columnCount(const QModelIndex& parent ) const
{
   Q_UNUSED(parent)
   return 18;
}

Qt::ItemFlags PhoneDirectoryModel::flags(const QModelIndex& index ) const
{
   Q_UNUSED(index)

   const PhoneNumber* number = m_lNumbers[index.row()];
   const bool enabled = !((index.column() == static_cast<int>(PhoneDirectoryModel::Columns::TRACKED)
      || static_cast<int>(PhoneDirectoryModel::Columns::PRESENT))
      && number->account() && (!number->account()->supportPresenceSubscribe()));

   return Qt::ItemIsEnabled
      | Qt::ItemIsSelectable 
      | (index.column() == static_cast<int>(PhoneDirectoryModel::Columns::TRACKED)&&enabled?Qt::ItemIsUserCheckable:Qt::NoItemFlags);
}

///This model is read and for debug purpose
bool PhoneDirectoryModel::setData(const QModelIndex& index, const QVariant &value, int role )
{
   PhoneNumber* number = m_lNumbers[index.row()];
   if (static_cast<PhoneDirectoryModel::Columns>(index.column())==PhoneDirectoryModel::Columns::TRACKED) {
      if (role == Qt::CheckStateRole && number) {
         number->setTracked(value.toBool());
      }
   }
   return false;
}

QVariant PhoneDirectoryModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
   Q_UNUSED(section)
   Q_UNUSED(orientation)
   static const QString headers[] = {tr("URI"), tr("Type"), tr("Contact"), tr("Account"), tr("State"), tr("Call count"), tr("Week count"),
   tr("Trimester count"), tr("Have Called"), tr("Last used"), tr("Name_count"),tr("Total (in seconds)"), tr("Popularity_index"), tr("Bookmarked"), tr("Tracked"), tr("Present"),
   tr("Presence message"), tr("Uid") };
   if (role == Qt::DisplayRole) return headers[section];
   return QVariant();
}

/**
 * This helper method make sure that number without an account get registered
 * correctly with their alternate URIs. In case there is an obvious duplication,
 * it will try to merge both numbers.
 */
void PhoneDirectoryModel::setAccount(PhoneNumber* number, Account* account ) {
   const URI& strippedUri = number->uri();
   const bool hasAtSign = strippedUri.hasHostname();
   number->setAccount(account);

   if (!hasAtSign) {
      NumberWrapper* wrap = m_hDirectory[strippedUri];

      //Check if a compatible number already exist
      if (wrap && wrap->numbers.indexOf(number) == -1) {
         qDebug() << "TODO";
      }

      //Let make sure none is created in the future for nothing
      if (!wrap) {
         wrap = new NumberWrapper();
         m_hDirectory[strippedUri+'@'+account->hostname()] = wrap;
      }
      wrap->numbers << number;

   }
}

PhoneNumber* PhoneDirectoryModel::getNumber(const QString& uri, const QString& type)
{
   const URI strippedUri(uri);
   NumberWrapper* wrap = m_hDirectory[strippedUri];
   if (wrap) {
      PhoneNumber* nb = wrap->numbers[0];
      if ((!nb->hasType()) && (!type.isEmpty())) {
         nb->setCategory(NumberCategoryModel::instance()->getCategory(type));
      }
      return nb;
   }

   //Too bad, lets create one
   PhoneNumber* number = new PhoneNumber(strippedUri,NumberCategoryModel::instance()->getCategory(type));
   number->setIndex(m_lNumbers.size());
   m_lNumbers << number;
   connect(number,SIGNAL(callAdded(Call*)),this,SLOT(slotCallAdded(Call*)));
   connect(number,SIGNAL(changed()),this,SLOT(slotChanged()));

   const QString hn = number->uri().hostname();

   //Check if we are lucky enough to have a single registered account with the same hostname
   /*if (!hn.isEmpty()) {
      
   }*/

   emit layoutChanged();
   if (!wrap) {
      wrap = new NumberWrapper();
      m_hDirectory[strippedUri] = wrap;
      m_hSortedNumbers[strippedUri] = wrap;
   }
   wrap->numbers << number;
   return number;
}

/**
 * This version of getNumber() try to get a phone number with a contact from an URI and account
 * It will also try to attach an account to existing numbers. This is not 100% reliable, but
 * it is correct often enough to do it.
 */
PhoneNumber* PhoneDirectoryModel::getNumber(const QString& uri, Account* account, const QString& type)
{
   if (!account)
      return getNumber(uri,type);

   const URI strippedUri(uri);

   //Try to use a PhoneNumber with a contact when possible, work only after the
   //contact are loaded
   bool hasContact = false;

   //See if the number is already loaded
   NumberWrapper* wrap  = m_hDirectory[strippedUri];
   NumberWrapper* wrap2 = nullptr;

   //Check if the URI is complete or short
   const bool hasAtSign = strippedUri.hasHostname();

   //Try to see if there is a better candidate with a suffix (LAN only)
   if ( !hasAtSign ) {
      //Append the account hostname
      wrap2 = m_hDirectory[strippedUri+'@'+account->hostname()];
   }

   if (wrap) {
      foreach(PhoneNumber* number, wrap->numbers) {
         //Not perfect, but better than ignoring the high probabilities
         //TODO only do it is hostname match
         if (!number->account())
            setAccount(number,account);
         if ((!number->hasType()) && (!type.isEmpty())) {
            number->setCategory(NumberCategoryModel::instance()->getCategory(type));
         }
         hasContact |= number->contact()!= nullptr;
      }
   }

   //If the second candidate has a contact, choose it (LAN only)
   if (wrap2 && (!hasContact)) {
      foreach(PhoneNumber* number, wrap2->numbers) {
         //Not perfect, but better than ignoring the high probabilities
         //TODO only do it is hostname match
         if (!number->account())
            setAccount(number,account);
         if ((!number->hasType()) && (!type.isEmpty())) {
            number->setCategory(NumberCategoryModel::instance()->getCategory(type));
         }
         if (number->contact()) {
            return number;
         }
      }
   }

   //Do the opposite, the URI has a suffix, try without
   if ((!wrap) && account && hasAtSign ) {
      const QString prefixOnly = strippedUri.userinfo();
      NumberWrapper* potentialwrap = m_hDirectory[prefixOnly];
      if (potentialwrap) {
         foreach(PhoneNumber* number, potentialwrap->numbers) {
            if (number->account() == account)
               return number;
         }
      }
   }

   //No better candidates were found than the original assumption, use it
   if (wrap) {
      foreach(PhoneNumber* number, wrap->numbers) {
         if ((!account) || number->account() == account)
            return number;
      }
   }

   //Create the number
   PhoneNumber* number = new PhoneNumber(strippedUri,NumberCategoryModel::instance()->getCategory(type));
   number->setAccount(account);
   number->setIndex( m_lNumbers.size());
   m_lNumbers << number;
   connect(number,SIGNAL(callAdded(Call*)),this,SLOT(slotCallAdded(Call*)));
   connect(number,SIGNAL(changed()),this,SLOT(slotChanged()));
   if (!wrap) {
      wrap = new NumberWrapper();
      m_hDirectory[strippedUri] = wrap;

      //Also add its alternative URI, it should be safe to do
      if ( !hasAtSign && !account->hostname().isEmpty() ) {
         if (!wrap2) {
            wrap2 = new NumberWrapper();
            m_hDirectory[strippedUri+'@'+account->hostname()] = wrap2;
         }
         wrap2->numbers << number;
      }

      m_hSortedNumbers[strippedUri] = wrap;
   }
   wrap->numbers << number;
   emit layoutChanged();
   return number;
}

PhoneNumber* PhoneDirectoryModel::getNumber(const QString& uri, Contact* contact, Account* account, const QString& type)
{
   if (!contact)
      return getNumber(uri,account,type);

   const URI strippedUri(uri);

   //See if the number is already loaded
   NumberWrapper* wrap = m_hDirectory[strippedUri];
   if (wrap) {
      //TODO find something better, it is prone to collisions
      foreach(PhoneNumber* number, wrap->numbers) {
         if (!number->contact()) {
            if (!number->account())
               setAccount(number,account);
            number->setContact(contact);
         }
      }
      foreach(PhoneNumber* number, wrap->numbers) {
         if ((!number->hasType()) && (!type.isEmpty())) {
            number->setCategory(NumberCategoryModel::instance()->getCategory(type));
         }
         //Use the operator== check to avoid issues with placeholders
         if (((!contact) || (number->contact() && (*number->contact()) == (*contact)))
            && ((!account) || number->account() == account))
            return number;
      }
   }

   //Create the number
   PhoneNumber* number = new PhoneNumber(strippedUri,NumberCategoryModel::instance()->getCategory(type));
   number->setAccount(account);
   number->setContact(contact);
   number->setIndex(m_lNumbers.size());
   m_lNumbers << number;
   connect(number,SIGNAL(callAdded(Call*)),this,SLOT(slotCallAdded(Call*)));
   connect(number,SIGNAL(changed()),this,SLOT(slotChanged()));
   if (!wrap) {
      wrap = new NumberWrapper();
      m_hDirectory[strippedUri] = wrap;
      m_hSortedNumbers[strippedUri] = wrap;
   }
   wrap->numbers << number;
   emit layoutChanged();
   return number;
}

PhoneNumber* PhoneDirectoryModel::fromTemporary(const TemporaryPhoneNumber* number)
{
   return getNumber(number->uri(),number->contact(),number->account());
}

PhoneNumber* PhoneDirectoryModel::fromHash(const QString& hash)
{
   const QStringList fields = hash.split("///");
   if (fields.size() == 3) {
      const QString uri = fields[0];
      Account* account = AccountListModel::instance()->getAccountById(fields[1]);
      Contact* contact = ContactModel::instance()->getContactByUid(fields[2].toUtf8());
      return getNumber(uri,contact,account);
   }
   else if (fields.size() == 1) {
      //FIXME Remove someday, handle version v1.0 to v1.2.3 bookmark format
      return getNumber(fields[0]);
   }
   qDebug() << "Invalid hash" << hash;
   return nullptr;
}

QVector<PhoneNumber*> PhoneDirectoryModel::getNumbersByPopularity() const
{
   return m_lPopularityIndex;
}

void PhoneDirectoryModel::slotCallAdded(Call* call)
{
   Q_UNUSED(call)
   PhoneNumber* number = qobject_cast<PhoneNumber*>(sender());
   if (number) {
      int currentIndex = number->popularityIndex();

      //The number is already in the top 10 and just passed the "index-1" one
      if (currentIndex > 0 && m_lPopularityIndex[currentIndex-1]->callCount() < number->callCount()) {
         do {
            PhoneNumber* tmp = m_lPopularityIndex[currentIndex-1];
            m_lPopularityIndex[currentIndex-1] = number;
            m_lPopularityIndex[currentIndex  ] = tmp   ;
            tmp->setPopularityIndex(tmp->popularityIndex()+1);
            currentIndex--;
         } while (currentIndex && m_lPopularityIndex[currentIndex-1]->callCount() < number->callCount());
         number->setPopularityIndex(currentIndex);
         emit layoutChanged();
      }
      //The top 10 is not complete, a call count of "1" is enough to make it
      else if (m_lPopularityIndex.size() < 10 && currentIndex == -1) {
         m_lPopularityIndex << number;
         number->setPopularityIndex(m_lPopularityIndex.size()-1);
         emit layoutChanged();
      }
      //The top 10 is full, but this number just made it to the top 10
      else if (currentIndex == -1 && m_lPopularityIndex.size() >= 10 && m_lPopularityIndex[9] != number && m_lPopularityIndex[9]->callCount() < number->callCount()) {
         PhoneNumber* tmp = m_lPopularityIndex[9];
         tmp->setPopularityIndex(-1);
         m_lPopularityIndex[9]     = number;
         number->setPopularityIndex(9);
         emit tmp->changed();
         emit number->changed();
      }

      //Now check for new peer names
      if (!call->peerName().isEmpty()) {
         number->incrementAlternativeName(call->peerName());
      }
   }
}

void PhoneDirectoryModel::slotChanged()
{
   PhoneNumber* number = qobject_cast<PhoneNumber*>(sender());
   if (number) {
      const int idx = number->index();
#ifndef NDEBUG
      if (idx<0)
         qDebug() << "Invalid slotChanged() index!" << idx;
#endif
      emit dataChanged(index(idx,0),index(idx,static_cast<int>(Columns::UID)));
   }
}

void PhoneDirectoryModel::slotNewBuddySubscription(const QString& accountId, const QString& uri, bool status, const QString& message)
{
   qDebug() << "New presence buddy" << uri << status << message;
   PhoneNumber* number = getNumber(uri,AccountListModel::instance()->getAccountById(accountId));
   number->setPresent(status);
   number->setPresenceMessage(message);
   emit number->changed();
}

// void PhoneDirectoryModel::slotStatusChanges(const QString& accountId, const QString& uri, bool status)
// {
//    qDebug() << "Presence status changed for" << uri << status;
//    PhoneNumber* number = getNumber(uri,AccountListModel::instance()->getAccountById(accountId));
//    number->m_Present = status;
//    number->m_PresentMessage = message;
//    emit number->changed();
// }

///Make sure the indexes are still valid for those names
void PhoneDirectoryModel::indexNumber(PhoneNumber* number, const QStringList &names)
{
   foreach(const QString& name, names) {
      const QString lower = name.toLower();
      const QStringList split = lower.split(' ');
      if (split.size() > 1) {
         foreach(const QString& chunk, split) {
            NumberWrapper* wrap = m_hNumbersByNames[chunk];
            if (!wrap) {
               wrap = new NumberWrapper();
               m_hNumbersByNames[chunk] = wrap;
               m_lSortedNames[chunk]    = wrap;
            }
            const int numCount = wrap->numbers.size();
            if (!((numCount == 1 && wrap->numbers[0] == number) || (numCount > 1 && wrap->numbers.indexOf(number) != -1)))
               wrap->numbers << number;
         }
      }
      NumberWrapper* wrap = m_hNumbersByNames[lower];
      if (!wrap) {
         wrap = new NumberWrapper();
         m_hNumbersByNames[lower] = wrap;
         m_lSortedNames[lower]    = wrap;
      }
      const int numCount = wrap->numbers.size();
      if (!((numCount == 1 && wrap->numbers[0] == number) || (numCount > 1 && wrap->numbers.indexOf(number) != -1)))
         wrap->numbers << number;
   }
}

int PhoneDirectoryModel::count() const {
   return m_lNumbers.size();
}
bool PhoneDirectoryModel::callWithAccount() const {
   return m_CallWithAccount;
}

//Setters
void PhoneDirectoryModel::setCallWithAccount(bool value) {
   m_CallWithAccount = value;
}
