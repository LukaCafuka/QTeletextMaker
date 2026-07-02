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

#include <QAbstractListModel>
#include <QList>
#include <QVariant>

#include "document.h"

#include "levelonepage.h"

QString TeletextDocument::formatPageNumber(int pageNumber)
{
	return QString::number(pageNumber & 0x7ff, 16).toUpper().rightJustified(3, QChar('0'));
}

ClutModel::ClutModel(QObject *parent): QAbstractListModel(parent)
{
	m_subPage = nullptr;
}

int ClutModel::rowCount(const QModelIndex & /*parent*/) const
{
	return 32;
}

QVariant ClutModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole)
		return QString("CLUT %1:%2").arg(index.row() >> 3).arg(index.row() & 0x07);

	if (role == Qt::DecorationRole && m_subPage != nullptr)
		return m_subPage->CLUTtoQColor(index.row());

	return QVariant();
}

void ClutModel::setSubPage(LevelOnePage *subPage)
{
	if (subPage != m_subPage) {
		m_subPage = subPage;
		emit dataChanged(createIndex(0, 0), createIndex(31, 0), QList<int>(Qt::DecorationRole));
	}
}


TeletextDocument::TeletextDocument()
{
	m_description.clear();
	m_currentPageIndex = 0;
	m_currentSubPageIndex = 0;
	m_undoStack = new QUndoStack(this);
	m_cursorRow = 1;
	m_cursorColumn = 0;
	m_rowZeroAllowed = false;
	m_selectionCornerRow = m_selectionCornerColumn = -1;
	m_selectionSubPage = nullptr;

	TeletextPageEntry initialPage;
	initialPage.pageNumber = 0x199;
	initialPage.subPages.append(new LevelOnePage);
	initialPage.subPageCodes.append(0);
	m_pages.append(initialPage);

	m_clutModel = new ClutModel;
	m_clutModel->setSubPage(m_pages[0].subPages[0]);
}

TeletextDocument::~TeletextDocument()
{
	delete m_clutModel;
	freeAllPages();

	for (auto &recycleSubPage : m_recycleSubPages)
		delete(recycleSubPage);
}

void TeletextDocument::freeAllPages()
{
	for (auto &pageEntry : m_pages) {
		for (auto &subPage : pageEntry.subPages)
			delete(subPage);
	}
	m_pages.clear();
}

bool TeletextDocument::isEmpty() const
{
	for (const auto &pageEntry : m_pages)
		for (auto &subPage : pageEntry.subPages)
			if (!subPage->isEmpty())
				return false;

	return true;
}

void TeletextDocument::clear()
{
	emit aboutToChangePage();
	emit aboutToChangeSubPage();

	freeAllPages();

	TeletextPageEntry initialPage;
	initialPage.pageNumber = 0x199;
	initialPage.subPages.append(new LevelOnePage);
	initialPage.subPageCodes.append(0);
	m_pages.append(initialPage);

	m_currentPageIndex = 0;
	m_currentSubPageIndex = 0;
	m_clutModel->setSubPage(m_pages[0].subPages[0]);
	emit pageSelected();
	emit subPageSelected();
	cancelSelection();
	m_undoStack->clear();
}

int TeletextDocument::subPageCodeAt(int pageIndex, int subPageIndex) const
{
	const TeletextPageEntry &pageEntry = m_pages.at(pageIndex);
	if (subPageIndex >= 0 && subPageIndex < pageEntry.subPageCodes.size())
		return pageEntry.subPageCodes.at(subPageIndex);
	return 0;
}

void TeletextDocument::selectPageIndex(int newPageIndex, bool forceRefresh)
{
	if (forceRefresh || (newPageIndex != m_currentPageIndex && newPageIndex >= 0 && newPageIndex < m_pages.size())) {
		emit aboutToChangePage();
		emit aboutToChangeSubPage();

		m_currentPageIndex = newPageIndex;
		if (m_currentSubPageIndex >= m_pages[m_currentPageIndex].subPages.size())
			m_currentSubPageIndex = m_pages[m_currentPageIndex].subPages.size() - 1;

		m_clutModel->setSubPage(m_pages[m_currentPageIndex].subPages[m_currentSubPageIndex]);
		emit pageSelected();
		emit subPageSelected();
		emit selectionMoved();
	}
}

void TeletextDocument::selectPageNext()
{
	if (m_currentPageIndex < m_pages.size() - 1)
		selectPageIndex(m_currentPageIndex + 1);
}

void TeletextDocument::selectPagePrevious()
{
	if (m_currentPageIndex > 0)
		selectPageIndex(m_currentPageIndex - 1);
}

void TeletextDocument::selectSubPageIndex(int newSubPageIndex, bool forceRefresh)
{
	// forceRefresh overrides "beyond the last subpage" check, so inserting a subpage after the last one still shows - dangerous workaround?
	if (forceRefresh || (newSubPageIndex != m_currentSubPageIndex && newSubPageIndex < m_pages[m_currentPageIndex].subPages.size())) {
		emit aboutToChangeSubPage();

		m_currentSubPageIndex = newSubPageIndex;

		m_clutModel->setSubPage(m_pages[m_currentPageIndex].subPages[m_currentSubPageIndex]);
		emit subPageSelected();
		emit selectionMoved();
		return;
	}
}

void TeletextDocument::selectSubPageNext()
{
	if (m_currentSubPageIndex < m_pages[m_currentPageIndex].subPages.size()-1) {
		emit aboutToChangeSubPage();

		m_currentSubPageIndex++;

		m_clutModel->setSubPage(m_pages[m_currentPageIndex].subPages[m_currentSubPageIndex]);
		emit subPageSelected();
		emit selectionMoved();
	}
}

void TeletextDocument::selectSubPagePrevious()
{
	if (m_currentSubPageIndex > 0) {
		emit aboutToChangeSubPage();

		m_currentSubPageIndex--;

		m_clutModel->setSubPage(m_pages[m_currentPageIndex].subPages[m_currentSubPageIndex]);
		emit subPageSelected();
		emit selectionMoved();
	}
}

void TeletextDocument::insertSubPage(int beforeSubPageIndex, bool copySubPage)
{
	TeletextPageEntry &pageEntry = m_pages[m_currentPageIndex];
	LevelOnePage *insertedSubPage;

	if (copySubPage)
		insertedSubPage = new LevelOnePage(*pageEntry.subPages.at(beforeSubPageIndex));
	else
		insertedSubPage = new LevelOnePage;

	const int subPageCode = beforeSubPageIndex < pageEntry.subPageCodes.size() ? pageEntry.subPageCodes.at(beforeSubPageIndex) : 0;

	if (beforeSubPageIndex == pageEntry.subPages.size()) {
		pageEntry.subPages.append(insertedSubPage);
		pageEntry.subPageCodes.append(subPageCode);
	} else {
		pageEntry.subPages.insert(beforeSubPageIndex, insertedSubPage);
		pageEntry.subPageCodes.insert(beforeSubPageIndex, subPageCode);
	}
}

void TeletextDocument::deleteSubPage(int subPageToDelete)
{
	m_clutModel->setSubPage(nullptr);

	delete(m_pages[m_currentPageIndex].subPages[subPageToDelete]);
	m_pages[m_currentPageIndex].subPages.remove(subPageToDelete);
	if (subPageToDelete < m_pages[m_currentPageIndex].subPageCodes.size())
		m_pages[m_currentPageIndex].subPageCodes.remove(subPageToDelete);
}

void TeletextDocument::deleteSubPageToRecycle(int subPageToRecycle)
{
	m_recycleSubPages.append(m_pages[m_currentPageIndex].subPages[subPageToRecycle]);
	m_pages[m_currentPageIndex].subPages.remove(subPageToRecycle);
	if (subPageToRecycle < m_pages[m_currentPageIndex].subPageCodes.size())
		m_pages[m_currentPageIndex].subPageCodes.remove(subPageToRecycle);
}

void TeletextDocument::unDeleteSubPageFromRecycle(int subPage)
{
	m_pages[m_currentPageIndex].subPages.insert(subPage, m_recycleSubPages.last());
	m_pages[m_currentPageIndex].subPageCodes.insert(subPage, 0);
	m_recycleSubPages.removeLast();
}

void TeletextDocument::loadFromList(QList<PageBase> const &subPageList)
{
	TeletextPageLoadData pageGroup;
	pageGroup.pageNumber = m_pages[m_currentPageIndex].pageNumber;
	pageGroup.subPages = subPageList;
	for (int i=0; i<subPageList.size(); i++)
		pageGroup.subPageCodes.append(i < m_pages[m_currentPageIndex].subPageCodes.size() ? m_pages[m_currentPageIndex].subPageCodes.at(i) : 0);
	loadFromPageGroups({ pageGroup });
}

void TeletextDocument::loadFromPageGroups(QList<TeletextPageLoadData> const &pageGroups)
{
	emit aboutToChangePage();
	emit aboutToChangeSubPage();

	freeAllPages();

	for (const TeletextPageLoadData &pageGroup : pageGroups) {
		TeletextPageEntry pageEntry;
		pageEntry.pageNumber = pageGroup.pageNumber;

		for (int i=0; i<pageGroup.subPages.size(); i++) {
			pageEntry.subPages.append(new LevelOnePage(pageGroup.subPages.at(i)));
			pageEntry.subPageCodes.append(i < pageGroup.subPageCodes.size() ? pageGroup.subPageCodes.at(i) : 0);
		}

		if (pageEntry.subPages.isEmpty()) {
			pageEntry.subPages.append(new LevelOnePage);
			pageEntry.subPageCodes.append(0);
		}

		m_pages.append(pageEntry);
	}

	if (m_pages.isEmpty()) {
		TeletextPageEntry initialPage;
		initialPage.pageNumber = 0x199;
		initialPage.subPages.append(new LevelOnePage);
		initialPage.subPageCodes.append(0);
		m_pages.append(initialPage);
	}

	m_currentPageIndex = 0;
	m_currentSubPageIndex = 0;
	m_clutModel->setSubPage(m_pages[0].subPages[0]);
	emit pageSelected();
	emit subPageSelected();
}

void TeletextDocument::loadMetaData(QVariantHash const &metadata)
{
	bool valueOk;

	if (const QString description = metadata.value("description").toString(); !description.isEmpty())
		m_description = description;

	if (const int pageNumber = metadata.value("pageNumber").toInt(&valueOk); valueOk)
		m_pages[m_currentPageIndex].pageNumber = pageNumber;

	if (metadata.value("fastextAbsolute").toBool()) {
		const int magazineFlip = m_pages[m_currentPageIndex].pageNumber & 0x700;

		for (auto &subPage : m_pages[m_currentPageIndex].subPages)
			for (int i=0; i<6; i++)
				subPage->setFastTextLinkPageNumber(i, subPage->fastTextLinkPageNumber(i) ^ magazineFlip);
	}

	for (int i=0; i<numberOfSubPages(); i++) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
		const QString subPageStr = QString("%1").arg(i, 3, '0');
#else
		const QString subPageStr = QString("%1").arg(i, 3, QChar('0'));
#endif

		if (int region = metadata.value("region" + subPageStr).toInt(&valueOk); valueOk)
			subPage(i)->setDefaultCharSet(region);
		if (int cycleValue = metadata.value("cycleValue" + subPageStr).toInt(&valueOk); valueOk)
			subPage(i)->setCycleValue(cycleValue);
		QChar cycleType = metadata.value("cycleType" + subPageStr).toChar();
		if (cycleType == 'C')
			subPage(i)->setCycleType(LevelOnePage::CTcycles);
		else if (cycleType == 'T')
			subPage(i)->setCycleType(LevelOnePage::CTseconds);
	}
}

void TeletextDocument::applyMagazineFlipToPage(TeletextPageEntry &pageEntry, int magazineFlip)
{
	if (!magazineFlip)
		return;

	for (auto &subPage : pageEntry.subPages) {
		for (int i=0; i<6; i++)
			subPage->setFastTextLinkPageNumber(i, subPage->fastTextLinkPageNumber(i) ^ magazineFlip);
		for (int i=0; i<8; i++)
			subPage->setComposeLinkPageNumber(i, subPage->composeLinkPageNumber(i) ^ magazineFlip);
	}
}

void TeletextDocument::setPageNumber(int pageNumber)
{
	TeletextPageEntry &pageEntry = m_pages[m_currentPageIndex];

	// If the magazine number was changed, we need to update the relative magazine numbers in FastText
	// and page enhancement links
	int oldMagazine = (pageEntry.pageNumber & 0xf00);
	int newMagazine = (pageNumber & 0xf00);
	// Fix magazine 0 to 8
	if (oldMagazine == 0x800)
		oldMagazine = 0x000;
	if (newMagazine == 0x800)
		newMagazine = 0x000;
	int magazineFlip = oldMagazine ^ newMagazine;

	pageEntry.pageNumber = pageNumber;
	applyMagazineFlipToPage(pageEntry, magazineFlip);
}

void TeletextDocument::setPageNumberFromString(QString pageNumberString)
{
	bool pageNumberOk;
	int pageNumberRead = pageNumberString.toInt(&pageNumberOk, 16);

	if ((!pageNumberOk) || pageNumberRead < 0x100 || pageNumberRead > 0x8ff)
		return;

	setPageNumber(pageNumberRead);
}

void TeletextDocument::setDescription(QString newDescription)
{
	m_description = newDescription;
}

void TeletextDocument::setFastTextLinkPageNumberOnAllSubPages(int linkNumber, int pageNumber)
{
	for (auto &subPage : m_pages[m_currentPageIndex].subPages)
		subPage->setFastTextLinkPageNumber(linkNumber, pageNumber);
}

void TeletextDocument::cursorUp(bool shiftKey)
{
	if (shiftKey && !selectionActive())
		setSelectionCorner(m_cursorRow, m_cursorColumn);

	if (--m_cursorRow == 0 - (int)m_rowZeroAllowed)
		m_cursorRow = 24;

	if (shiftKey)
		emit selectionMoved();
	else
		cancelSelection();

	emit cursorMoved();
}

void TeletextDocument::cursorDown(bool shiftKey)
{
	if (shiftKey && !selectionActive())
		setSelectionCorner(m_cursorRow, m_cursorColumn);

	if (++m_cursorRow == 25)
		m_cursorRow = (int)!m_rowZeroAllowed;

	if (shiftKey)
		emit selectionMoved();
	else
		cancelSelection();

	emit cursorMoved();
}

void TeletextDocument::cursorLeft(bool shiftKey)
{
	if (shiftKey && !selectionActive())
		setSelectionCorner(m_cursorRow, m_cursorColumn);

	if (--m_cursorColumn == -1) {
		m_cursorColumn = 39;
		cursorUp(shiftKey);
	}

	if (shiftKey)
		emit selectionMoved();
	else
		cancelSelection();

	emit cursorMoved();
}

void TeletextDocument::cursorRight(bool shiftKey)
{
	if (shiftKey && !selectionActive())
		setSelectionCorner(m_cursorRow, m_cursorColumn);

	if (++m_cursorColumn == 40) {
		m_cursorColumn = 0;
		cursorDown(shiftKey);
	}

	if (shiftKey)
		emit selectionMoved();
	else
		cancelSelection();

	emit cursorMoved();
}

void TeletextDocument::moveCursor(int cursorRow, int cursorColumn, bool selectionInProgress)
{
	if (selectionInProgress && !selectionActive())
		setSelectionCorner(m_cursorRow, m_cursorColumn);

	if (cursorRow != -1)
		m_cursorRow = cursorRow;
	if (cursorColumn != -1)
		m_cursorColumn = cursorColumn;

	if (selectionInProgress)
		emit selectionMoved();
	else
		cancelSelection();

	emit cursorMoved();
}

void TeletextDocument::setRowZeroAllowed(bool allowed)
{
	m_rowZeroAllowed = allowed;
	if (m_cursorRow == 0 && !allowed)
		cursorDown();
}

void TeletextDocument::setSelectionCorner(int row, int column)
{
	if (m_selectionCornerRow != row || m_selectionCornerColumn != column) {
		m_selectionSubPage = currentSubPage();
		m_selectionCornerRow = row;
		m_selectionCornerColumn = column;

//		emit selectionMoved();
	}
}

void TeletextDocument::setSelection(int topRow, int leftColumn, int bottomRow, int rightColumn)
{
	if (selectionTopRow() != topRow || selectionBottomRow() != bottomRow || selectionLeftColumn() != leftColumn || selectionRightColumn() != rightColumn) {
		m_selectionSubPage = currentSubPage();
		m_selectionCornerRow = topRow;
		m_cursorRow = bottomRow;
		m_selectionCornerColumn = leftColumn;
		m_cursorColumn = rightColumn;

		emit selectionMoved();
		emit cursorMoved();
	}
}

void TeletextDocument::cancelSelection()
{
	if (m_selectionSubPage != nullptr) {
		m_selectionSubPage = nullptr;
		emit selectionMoved();
		m_selectionCornerRow = m_selectionCornerColumn = -1;
	}
}

int TeletextDocument::levelRequired() const
{
	int levelSeen = 0;

	for (const auto &pageEntry : m_pages)
		for (auto &subPage : pageEntry.subPages) {
			levelSeen = qMax(levelSeen, subPage->levelRequired());
			if (levelSeen == 3)
				return levelSeen;
		}

	return levelSeen;
}
