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

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QUndoStack>
#include <QVariant>

#include "levelonepage.h"

class ClutModel : public QAbstractListModel
{
	Q_OBJECT

public:
	ClutModel(QObject *parent = 0);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	void setSubPage(LevelOnePage *page);

private:
	LevelOnePage *m_subPage;
};

struct TeletextPageLoadData
{
	int pageNumber = 0x199;
	QList<PageBase> subPages;
	QList<int> subPageCodes;
};

struct TeletextPageEntry
{
	int pageNumber = 0x199;
	QList<LevelOnePage *> subPages;
	QList<int> subPageCodes;
};

class TeletextDocument : public QObject
{
	Q_OBJECT

public:
	TeletextDocument();
	~TeletextDocument();

	bool isEmpty() const;
	void clear();

	int numberOfPages() const { return m_pages.size(); }
	int currentPageIndex() const { return m_currentPageIndex; }
	int pageNumberAt(int pageIndex) const { return m_pages.at(pageIndex).pageNumber; }
	int subPageCodeAt(int pageIndex, int subPageIndex) const;
	const TeletextPageEntry &pageEntry(int pageIndex) const { return m_pages.at(pageIndex); }
	void selectPageIndex(int newPageIndex, bool forceRefresh=false);
	void selectPageNext();
	void selectPagePrevious();

	int numberOfSubPages() const { return m_pages[m_currentPageIndex].subPages.size(); }
	LevelOnePage* subPage(int p) const { return m_pages[m_currentPageIndex].subPages[p]; }
	LevelOnePage* currentSubPage() const { return m_pages[m_currentPageIndex].subPages[m_currentSubPageIndex]; }
	int currentSubPageIndex() const { return m_currentSubPageIndex; }
	void selectSubPageIndex(int newSubPageIndex, bool refresh=false);
	void selectSubPageNext();
	void selectSubPagePrevious();
	void insertSubPage(int beforeSubPageIndex, bool copySubPage);
	void deleteSubPage(int subPageToDelete);
	void deleteSubPageToRecycle(int subPageToRecycle);
	void unDeleteSubPageFromRecycle(int subPage);
	void loadFromList(QList<PageBase> const &subPageList);
	void loadFromPageGroups(QList<TeletextPageLoadData> const &pageGroups);
	void loadMetaData(QVariantHash const &metadata);
	static QString formatPageNumber(int pageNumber);
	int pageNumber() const { return m_pages[m_currentPageIndex].pageNumber; }
	void setPageNumber(int pageNumber);
	void setPageNumberFromString(QString pageNumberString);
	QString description() const { return m_description; }
	void setDescription(QString newDescription);
	void setFastTextLinkPageNumberOnAllSubPages(int linkNumber, int pageNumber);
	QUndoStack *undoStack() const { return m_undoStack; }
	ClutModel *clutModel() const { return m_clutModel; }
	int cursorRow() const { return m_cursorRow; }
	int cursorColumn() const { return m_cursorColumn; }
	void cursorUp(bool shiftKey=false);
	void cursorDown(bool shiftKey=false);
	void cursorLeft(bool shiftKey=false);
	void cursorRight(bool shiftKey=false);
	void moveCursor(int cursorRow, int cursorColumn, bool selectionInProgress=false);
	bool rowZeroAllowed() const { return m_rowZeroAllowed; };
	void setRowZeroAllowed(bool allowed);
	int selectionTopRow() const { return m_selectionCornerRow == -1 ? m_cursorRow : qMin(m_selectionCornerRow, m_cursorRow); }
	int selectionBottomRow() const { return qMax(m_selectionCornerRow, m_cursorRow); }
	int selectionLeftColumn() const { return m_selectionCornerColumn == -1 ? m_cursorColumn : qMin(m_selectionCornerColumn, m_cursorColumn); }
	int selectionRightColumn() const { return qMax(m_selectionCornerColumn, m_cursorColumn); }
	int selectionWidth() const { return m_selectionCornerColumn == -1 ? 1 : selectionRightColumn() - selectionLeftColumn() + 1; }
	int selectionHeight() const { return m_selectionCornerRow == -1 ? 1 : selectionBottomRow() - selectionTopRow() + 1; }
	bool selectionActive() const { return m_selectionSubPage == currentSubPage(); }
	int selectionCornerRow() const { return m_selectionCornerRow == -1 ? m_cursorRow : m_selectionCornerRow; }
	int selectionCornerColumn() const { return m_selectionCornerColumn == -1 ? m_cursorColumn : m_selectionCornerColumn; }
	void setSelectionCorner(int row, int column);
	void setSelection(int topRow, int leftColumn, int bottomRow, int rightColumn);
	void cancelSelection();
	int levelRequired() const;

signals:
	void cursorMoved();
	void selectionMoved();
	void colourChanged(int i);
	void dClutChanged(bool g, int m, int i);
	void pageOptionsChanged();
	void aboutToChangeSubPage();
	void subPageSelected();
	void aboutToChangePage();
	void pageSelected();
	void contentsChanged();

	void tripletCommandHighlight(int tripletNumber);

private:
	void freeAllPages();
	void applyMagazineFlipToPage(TeletextPageEntry &pageEntry, int magazineFlip);

	QString m_description;
	int m_currentPageIndex, m_currentSubPageIndex;
	QList<TeletextPageEntry> m_pages;
	QList<LevelOnePage *> m_recycleSubPages;
	QUndoStack *m_undoStack;
	int m_cursorRow, m_cursorColumn, m_selectionCornerRow, m_selectionCornerColumn;
	bool m_rowZeroAllowed;
	LevelOnePage *m_selectionSubPage;
	ClutModel *m_clutModel;
};

#endif
