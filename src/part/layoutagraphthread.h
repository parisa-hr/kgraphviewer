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

#ifndef LAYOUTAGRAPHTHREAD_H
#define LAYOUTAGRAPHTHREAD_H

#include <QSemaphore>
#include <QThread>

#include <graphviz/gvc.h>

int threadsafe_wrap_gvLayout(GVC_t *gvc, graph_t *g, const char *engine);
int threadsafe_wrap_gvRender(GVC_t *gvc, graph_t *g, const char *format, FILE *out);

class LayoutAGraphThread : public QThread
{
    Q_OBJECT
public:
    LayoutAGraphThread();
    ~LayoutAGraphThread() override;
    void layoutGraph(graph_t *graph, const QString &layoutCommand);
    inline graph_t *g()
    {
        return m_g;
    }
    inline GVC_t *gvc()
    {
        return m_gvc;
    }
    inline const QString &layoutCommand() const
    {
        return m_layoutCommand;
    }
    void processed_finished()
    {
        sem.release();
    }

protected:
    void run() override;

private:
    QSemaphore sem;
    QString m_layoutCommand;
    graph_t *m_g = nullptr;
    GVC_t *m_gvc = nullptr;
};

#endif // LAYOUTAGRAPHTHREAD_H
