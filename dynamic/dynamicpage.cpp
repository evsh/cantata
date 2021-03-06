/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "dynamicpage.h"
#include "dynamic.h"
#include "dynamicrulesdialog.h"
#include "support/localize.h"
#include "widgets/icons.h"
#include "support/action.h"
#include "support/configuration.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "gui/stdactions.h"

DynamicPage::DynamicPage(QWidget *p)
    : SinglePageWidget(p)
{
    addAction = new Action(Icons::self()->addNewItemIcon, i18n("Add"), this);
    editAction = new Action(Icons::self()->editIcon, i18n("Edit"), this);
    removeAction = new Action(Icons::self()->removeDynamicIcon, i18n("Remove"), this);
    toggleAction = new Action(this);

    ToolButton *addBtn=new ToolButton(this);
    ToolButton *editBtn=new ToolButton(this);
    ToolButton *removeBtn=new ToolButton(this);
    ToolButton *startBtn=new ToolButton(this);

    addBtn->setDefaultAction(addAction);
    editBtn->setDefaultAction(editAction);
    removeBtn->setDefaultAction(removeAction);
    startBtn->setDefaultAction(Dynamic::self()->startAct());

    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(Dynamic::self()->startAct());
    view->alwaysShowHeader();

    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(toggle()));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(MPDConnection::self(), SIGNAL(dynamicSupport(bool)), this, SLOT(remoteDynamicSupport(bool)));
    connect(addAction, SIGNAL(triggered()), SLOT(add()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(removeAction, SIGNAL(triggered()), SLOT(remove()));
    connect(Dynamic::self()->startAct(), SIGNAL(triggered()), SLOT(start()));
    connect(Dynamic::self()->stopAct(), SIGNAL(triggered()), SLOT(stop()));
    connect(toggleAction, SIGNAL(triggered()), SLOT(toggle()));
    connect(Dynamic::self(), SIGNAL(running(bool)), SLOT(running(bool)));
    connect(Dynamic::self(), SIGNAL(loadingList()), view, SLOT(showSpinner()));
    connect(Dynamic::self(), SIGNAL(loadedList()), view, SLOT(hideSpinner()));

    #ifdef Q_OS_WIN
    remoteRunningLabel=new QLabel(this);
    remoteRunningLabel->setStyleSheet(QString(".QLabel {"
                          "background-color: rgba(235, 187, 187, 196);"
                          "border-radius: 3px;"
                          "border: 1px solid red;"
                          "padding: 4px;"
                          "margin: 1px;"
                          "color: black; }"));
    remoteRunningLabel->setText(i18n("Remote dynamizer is not running."));
    #endif
    Dynamic::self()->stopAct()->setEnabled(false);
    proxy.setSourceModel(Dynamic::self());
    view->setModel(&proxy);
    view->setDeleteAction(removeAction);
    view->setMode(ItemView::Mode_List);
    controlActions();
    Configuration config(metaObject()->className());
    view->load(config);
    controls=QList<QWidget *>() << addBtn << editBtn << removeBtn << startBtn;
    init(0, QList<QWidget *>(), controls);
    #ifdef Q_OS_WIN
    addWidget(remoteRunningLabel);
    enableWidgets(false);
    #endif
}

DynamicPage::~DynamicPage()
{
    Configuration config(metaObject()->className());
    view->save(config);
}

void DynamicPage::doSearch()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !proxy.filterText().isEmpty()) {
        view->expandAll();
    }
}

void DynamicPage::controlActions()
{
    QModelIndexList selected=qobject_cast<Dynamic *>(sender()) ? QModelIndexList() : view->selectedIndexes(false); // Dont need sorted selection here...

    editAction->setEnabled(1==selected.count());
    Dynamic::self()->startAct()->setEnabled(1==selected.count());
    removeAction->setEnabled(selected.count());
}

void DynamicPage::remoteDynamicSupport(bool s)
{
    #ifdef Q_OS_WIN
    remoteRunningLabel->setVisible(!s);
    enableWidgets(s);
    #endif
    view->setBackgroundImage(s ? Icon(QStringList() << "network-server-database.svg" << "applications-internet") : Icon());
}

void DynamicPage::add()
{
    DynamicRulesDialog *dlg=new DynamicRulesDialog(this);
    dlg->edit(QString());
}

void DynamicPage::edit()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    DynamicRulesDialog *dlg=new DynamicRulesDialog(this);
    dlg->edit(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::remove()
{
    QModelIndexList selected=view->selectedIndexes();

    if (selected.isEmpty() ||
        MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected rules?\n\nThis cannot be undone."),
                                                 i18n("Remove Dynamic Rules"), StdGuiItem::remove(), StdGuiItem::cancel())) {
        return;
    }

    QStringList names;
    foreach (const QModelIndex &idx, selected) {
        names.append(idx.data(Qt::DisplayRole).toString());
    }

    foreach (const QString &name, names) {
        Dynamic::self()->del(name);
    }
}

void DynamicPage::start()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }
    Dynamic::self()->start(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::stop()
{
    Dynamic::self()->stop();
}

void DynamicPage::toggle()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    Dynamic::self()->toggle(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::running(bool status)
{
    Dynamic::self()->stopAct()->setEnabled(status);
}

void DynamicPage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void DynamicPage::enableWidgets(bool enable)
{
    foreach (QWidget *c, controls) {
        c->setEnabled(enable);
    }

    view->setEnabled(enable);
}

void DynamicPage::showEvent(QShowEvent *e)
{
    view->focusView();
    Dynamic::self()->enableRemotePolling(true);
    SinglePageWidget::showEvent(e);
}

void DynamicPage::hideEvent(QHideEvent *e)
{
    Dynamic::self()->enableRemotePolling(false);
    SinglePageWidget::hideEvent(e);
}
