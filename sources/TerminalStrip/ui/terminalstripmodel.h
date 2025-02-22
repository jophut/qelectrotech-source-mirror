/*
        Copyright 2006-2021 The QElectroTech Team
        This file is part of QElectroTech.

        QElectroTech is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 2 of the License, or
        (at your option) any later version.

        QElectroTech is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef TERMINALSTRIPMODEL_H
#define TERMINALSTRIPMODEL_H

#include <QAbstractTableModel>
#include <QObject>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QPair>

#include "../terminalstrip.h"

class TerminalStrip;

class TerminalStripModel : public QAbstractTableModel
{
        Q_OBJECT
    public:
        TerminalStripModel(TerminalStrip *terminal_strip, QObject *parent = nullptr);

        virtual int rowCount    (const QModelIndex &parent = QModelIndex()) const override;
        virtual int columnCount (const QModelIndex &parent = QModelIndex()) const override;
        virtual QVariant data   (const QModelIndex &index, int role = Qt::DisplayRole) const override;
		virtual bool setData (const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
		virtual QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
		virtual Qt::ItemFlags flags (const QModelIndex &index) const override;

		QVector<QPair<RealTerminalData, RealTerminalData>> modifiedRealTerminalData() const;

		bool isXrefCell(const QModelIndex &index, Element **element = nullptr);
		QVector<PhysicalTerminalData> physicalTerminalDataForIndex(QModelIndexList index_list) const;
		QVector<RealTerminalData> realTerminalDataForIndex(QModelIndexList index_list) const;

	private:
		void fillPhysicalTerminalData();
		RealTerminalData dataAtRow(int row) const;
		void replaceDataAtRow(RealTerminalData data, int row);
		PhysicalTerminalData physicalDataAtIndex(int index) const;
		RealTerminalData realDataAtIndex(int index) const;

    private:
        QPointer<TerminalStrip> m_terminal_strip;
		QVector<PhysicalTerminalData> m_edited_terminal_data, m_original_terminal_data;
		QHash<Element *, QVector<bool>> m_modified_cell;
};

class TerminalStripModelDelegate : public QStyledItemDelegate
{
	Q_OBJECT

	public:
		TerminalStripModelDelegate(QObject *parent = Q_NULLPTR);

		QWidget *createEditor(
				QWidget *parent,
				const QStyleOptionViewItem &option,
				const QModelIndex &index) const override;
		void setModelData(
				QWidget *editor,
				QAbstractItemModel *model,
				const QModelIndex &index) const override;
};

#endif // TERMINALSTRIPMODEL_H
