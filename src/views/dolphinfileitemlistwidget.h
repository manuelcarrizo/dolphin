/***************************************************************************
 *   Copyright (C) 2011 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#ifndef DOLPHINFILEITEMLISTWIDGET_H
#define DOLPHINFILEITEMLISTWIDGET_H

#include "dolphin_export.h"

#include <kitemviews/kfileitemlistwidget.h>
#include "versioncontrol/kversioncontrolplugin.h"

/**
 * @brief Extends KFileItemListWidget to handle the "version" role.
 *
 * The "version" role is set if version-control-plugins have been enabled.
 * @see KVersionControlPlugin
 */
class DOLPHIN_EXPORT DolphinFileItemListWidget : public KFileItemListWidget
{
    Q_OBJECT

public:
    DolphinFileItemListWidget(KItemListWidgetInformant* informant, QGraphicsItem* parent);
    virtual ~DolphinFileItemListWidget();

protected:
    virtual void refreshCache() Q_DECL_OVERRIDE;

private:
    static QPixmap overlayForState(KVersionControlPlugin::ItemVersion version, int size);

};

#endif


