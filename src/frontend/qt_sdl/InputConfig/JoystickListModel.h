/*
    Copyright 2016-2025 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef JOYSTICKLISTMODEL_H
#define JOYSTICKLISTMODEL_H
#include <QAbstractListModel>
#include <SDL3/SDL_guid.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_oldnames.h>


class JoystickListModel : public QAbstractListModel
{
    struct Entry
    {
        SDL_JoystickID id;
        SDL_GUID guid;
        QString name;
    };

public:
    explicit JoystickListModel(QObject* parent) : QAbstractListModel(parent)
    {
        int count;
        SDL_JoystickID* ids = SDL_GetJoysticks(&count);

        for (int i = 0; i < count; i++)
        {
            SDL_JoystickID id = ids[i];
            const char* name = SDL_GetJoystickNameForID(id);
            const SDL_GUID guid = SDL_GetJoystickGUIDForID(id);

            joysticks.append({ id, guid, QString(name) });
        }
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return {};

        if (role == Qt::DisplayRole && index.row() < joysticks.count())
            return joysticks[index.row()].name;

        return {};
    }

    int rowCount(const QModelIndex&parent) const override
    {
        return joysticks.count();
    }

    SDL_JoystickID getInstanceID(const int index)
    {
        return joysticks[index].id;
    }

    SDL_GUID getGUID(const int index)
    {
        return joysticks[index].guid;
    }

    int getIndexByID(SDL_JoystickID id)
    {
        printf("find index of %d\n", id);
        for (int i = 0; i < joysticks.count(); i++)
        {
            if (joysticks[i].id == id)
            {
                printf("found %d\n", i);
                return i;
            }
        }
        printf("not found\n");

        return -1;
    }

private:
    QList<Entry> joysticks;
};


#endif //JOYSTICKLISTMODEL_H