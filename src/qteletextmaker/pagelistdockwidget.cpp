/*
 * Copyright (C) 2020-2026 Gavin MacGregor
 *
 * This file is part of QTeletextMaker.
 *
 * QTeletextMaker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QTeletextMaker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QTeletextMaker.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "pagelistdockwidget.h"

#include <QListWidgetItem>

PageListDockWidget::PageListDockWidget(TeletextWidget *parent) : QDockWidget(tr("Pages"), parent)
{
	m_parentMainWidget = parent;

	m_pageListWidget = new QListWidget(this);
	setWidget(m_pageListWidget);

	connect(m_pageListWidget, &QListWidget::currentRowChanged, this, [=](int row) {
		if (row >= 0 && row != m_parentMainWidget->document()->currentPageIndex())
			m_parentMainWidget->document()->selectPageIndex(row);
	});

	updatePageList();
}

void PageListDockWidget::updatePageList()
{
	const TeletextDocument *document = m_parentMainWidget->document();
	const int currentPageIndex = document->currentPageIndex();

	m_pageListWidget->blockSignals(true);
	m_pageListWidget->clear();

	for (int i=0; i<document->numberOfPages(); i++) {
		const int pageNumber = document->pageNumberAt(i);
		const int subPageCount = document->pageEntry(i).subPages.size();
		QString label = QString("P%1").arg(TeletextDocument::formatPageNumber(pageNumber));
		if (subPageCount > 1)
			label.append(QString(" (%1)").arg(subPageCount));
		m_pageListWidget->addItem(label);
	}

	if (currentPageIndex >= 0 && currentPageIndex < m_pageListWidget->count())
		m_pageListWidget->setCurrentRow(currentPageIndex);

	m_pageListWidget->blockSignals(false);
}
