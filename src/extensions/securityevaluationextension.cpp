/****************************************************************************
 *   Copyright (C) 2015 by Savoir-Faire Linux                               *
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
#include "securityevaluationextension.h"
#include "collectioninterface.h"
#include "contactmethod.h"
#include "person.h"
#include "presencestatusmodel.h"
#include "collectionextensionmodel.h"

DECLARE_COLLECTION_EXTENSION(SecurityEvaluationExtension)

SecurityEvaluationExtension::SecurityEvaluationExtension(QObject* parent) :
   CollectionExtensionInterface(parent)
{

}

QVariant SecurityEvaluationExtension::data(int role) const
{
   Q_UNUSED(role)

   if (role == Qt::DisplayRole) {
      return QObject::tr("Security evaluation");
   }

   return QVariant();
}
