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
#include "sortterminalstripcommand.h"
#include "../terminalstrip.h"

SortTerminalStripCommand::SortTerminalStripCommand(TerminalStrip *strip, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_strip(strip)
{
	setText(QObject::tr("Trier le bornier %1").arg(m_strip->name()));
	m_old_order = m_new_order = m_strip->physicalTerminalData();
	sort();
}

void SortTerminalStripCommand::undo()
{
	if (m_strip) {
		m_strip->setOrderTo(m_old_order);
	}
}

void SortTerminalStripCommand::redo()
{
	if (m_strip) {
		m_strip->setOrderTo(m_new_order);
	}
}

void SortTerminalStripCommand::sort()
{
	std::sort(m_new_order.begin(), m_new_order.end(), [](PhysicalTerminalData arg1, PhysicalTerminalData arg2)
	{
		const QRegularExpression rx(QStringLiteral("^\\d+"));

		QString str1;
		QString str2;
		int int1 =-1;
		int int2 =-1;

		if (arg1.real_terminals_vector.count())
		{
			str1 = arg1.real_terminals_vector.constLast().label_;

			auto match = rx.match(str1);
			if (match.hasMatch()) {
				int1 = match.captured(0).toInt();
			}
		}

		if (arg2.real_terminals_vector.count())
		{
			str2 = arg2.real_terminals_vector.constLast().label_;

			auto match = rx.match(str2);
			if (match.hasMatch()) {
				int2 = match.captured(0).toInt();
			}
		}

			//Sort as numbers if both string
			//start at least by a digit and
			//the number of each string are different.
			//Else sort as string
		if (int1 >= 0 &&
			int2 >= 0 &&
			int1 != int2) {
			return int1<int2;
		}
		else {
			return str1<str2;
		}
	});
}
