/*
    This file is part of KGraphViewer.
    Copyright (C) 2010  Gael de Chalendar <kleag@free.fr>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef LOADAGRAPHTHREAD_H
#define LOADAGRAPHTHREAD_H

#include <QSemaphore>
#include <QThread>

#include <graphviz/gvc.h>

class LoadAGraphThread : public QThread
{
    Q_OBJECT
public:
    LoadAGraphThread()
        : sem(1)
    {
    }
    void loadFile(const QString &dotFileName);
    inline graph_t *g()
    {
        return m_g;
    }
    inline const QString &dotFileName()
    {
        return m_dotFileName;
    }
    void processed_finished()
    {
        sem.release();
    }

    // helper method only for DotGraphView::loadLibrarySync()
    // see notes next to the call there
    void setDotFileName(const QString &dotFileName)
    {
        m_dotFileName = dotFileName;
    }

protected:
    void run() override;

private:
    QSemaphore sem;
    QString m_dotFileName;
    graph_t *m_g = nullptr;
};

#endif // LOADAGRAPHTHREAD_H
