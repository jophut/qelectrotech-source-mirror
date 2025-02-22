﻿/*
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
#include "terminalstripeditor.h"
#include "ui_terminalstripeditor.h"
#include "terminalstripcreatordialog.h"
#include "../../qetproject.h"
#include "../terminalstrip.h"
#include "../elementprovider.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../UndoCommand/addterminalstripcommand.h"
#include "../UndoCommand/addterminaltostripcommand.h"
#include "../UndoCommand/changeterminalstripdata.h"
#include "../undocommand/changeelementdatacommand.h"
#include "terminalstriptreewidget.h"
#include "../../qeticons.h"
#include "terminalstripmodel.h"
#include "../diagram.h"
#include "../UndoCommand/sortterminalstripcommand.h"
#include "../UndoCommand/groupterminalscommand.h"
#include "../UndoCommand/changeterminallevel.h"

#include <QTreeWidgetItem>

/**
 * @brief TerminalStripEditor::TerminalStripEditor
 * @param project : Project to manage the terminal strip
 * @param parent : paent widget
 */
TerminalStripEditor::TerminalStripEditor(QETProject *project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::TerminalStripEditor),
	m_project(project)
{
	ui->setupUi(this);

	ui->m_table_widget->setItemDelegate(new TerminalStripModelDelegate(ui->m_terminal_strip_tw));
	ui->m_remove_terminal_strip_pb->setDisabled(true);
	buildTree();
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	ui->m_terminal_strip_tw->expandRecursively(ui->m_terminal_strip_tw->rootIndex());
#else
	ui->m_terminal_strip_tw->expandAll();
#endif
	setUpUndoConnections();

		//Call for update the state of child widgets
	selectionChanged();

		//Go the diagram of double clicked terminal
	connect(ui->m_table_widget, &QAbstractItemView::doubleClicked, [this](const QModelIndex &index)
	{
		Element *elmt = nullptr;
		if (this->m_model->isXrefCell(index, &elmt))
		{
			auto diagram = elmt->diagram();
			if (diagram)
			{
				diagram->showMe();
				if (diagram->views().size())
				{
					auto fit_view = elmt->sceneBoundingRect();
					fit_view.adjust(-200,-200,200,200);
					diagram->views().first()->fitInView(fit_view, Qt::KeepAspectRatioByExpanding);
				}
			}
		}
	});
}

/**
 * @brief TerminalStripEditor::~TerminalStripEditor
 */
TerminalStripEditor::~TerminalStripEditor() {
	delete ui;
}

void TerminalStripEditor::setUpUndoConnections()
{
	connect(ui->m_terminal_strip_tw, &TerminalStripTreeWidget::terminalAddedToStrip,
			[this](QUuid terminal_uuid, QUuid strip_uuid)
	{
		auto terminal = m_uuid_terminal_H.value(terminal_uuid);
		auto strip = m_uuid_strip_H.value(strip_uuid);

		if (!terminal || !strip) {
			return;
		}

		auto undo = new AddTerminalToStripCommand(terminal, strip);
		m_project->undoStack()->push(undo);
	});

	connect(ui->m_terminal_strip_tw, &TerminalStripTreeWidget::terminalMovedFromStripToStrip,
			[this] (QUuid terminal_uuid, QUuid old_strip_uuid, QUuid new_strip_uuid)
	{
		auto terminal  = m_uuid_terminal_H.value(terminal_uuid);
		auto old_strip = m_uuid_strip_H.value(old_strip_uuid);
		auto new_strip = m_uuid_strip_H.value(new_strip_uuid);

		if (!terminal || !old_strip || !new_strip) {
			return;
		}

		auto undo = new AddTerminalToStripCommand(terminal, old_strip, new_strip);
		m_project->undoStack()->push(undo);
	});

	connect(ui->m_terminal_strip_tw, &TerminalStripTreeWidget::terminalRemovedFromStrip,
			[this] (QUuid terminal_uuid, QUuid old_strip_uuid)
	{
		auto terminal_ = m_uuid_terminal_H.value(terminal_uuid);
		auto strip_ = m_uuid_strip_H.value(old_strip_uuid);

		if (!terminal_ || !strip_) {
			return;
		}

		auto undo = new RemoveTerminalFromStripCommand(terminal_, strip_);
		m_project->undoStack()->push(undo);
	});
}

/**
 * @brief TerminalStripEditor::buildTree
 * Build the tree widget use to explore terminal strip
 */
void TerminalStripEditor::buildTree()
{
	ui->m_terminal_strip_tw->clear();

	auto title = m_project->title();
	if (title.isEmpty()) {
		title = tr("Projet sans titre");
	}

	QStringList strl{title};
	new QTreeWidgetItem(ui->m_terminal_strip_tw, strl, TerminalStripTreeWidget::Root);

	QStringList ftstrl(tr("Bornes indépendante"));
	new QTreeWidgetItem(ui->m_terminal_strip_tw, ftstrl, TerminalStripTreeWidget::FreeTerminal);

	auto ts_vector = m_project->terminalStrip();
	std::sort(ts_vector.begin(), ts_vector.end(), [](TerminalStrip *a, TerminalStrip *b) {
		return a->name() < b->name();});

	for (const auto ts : qAsConst(ts_vector)) {
		addTerminalStrip(ts);
	}
	addFreeTerminal();
}

/**
 * @brief TerminalStripEditor::addTerminalStrip
 * Add a new terminal strip to the list of displayed terminal strip
 * in the tree widget
 * @param terminal_strip
 * @return the QTreeWidgetItem who represent the terminal strip
 * both if created or already exist
 */
QTreeWidgetItem* TerminalStripEditor::addTerminalStrip(TerminalStrip *terminal_strip)
{
	if (auto item = m_item_strip_H.key(terminal_strip)) {
		return item;
	}

	auto root_item = ui->m_terminal_strip_tw->topLevelItem(0);

		//Check if installation already exist
		//if not create a new one
	auto installation_str = terminal_strip->installation();
	QTreeWidgetItem *inst_qtwi = nullptr;
	for (int i = 0 ; i<root_item->childCount() ; ++i) {
		auto child_inst = root_item->child(i);
		if (child_inst->data(0, Qt::DisplayRole).toString() == installation_str) {
			inst_qtwi = child_inst;
			break;
		}
	}
	if (!inst_qtwi) {
		QStringList inst_strl{installation_str};
		inst_qtwi = new QTreeWidgetItem(root_item, inst_strl, TerminalStripTreeWidget::Installation);
	}

		//Check if location already exist
		//if not create a new one
	auto location_str = terminal_strip->location();
	QTreeWidgetItem *loc_qtwi = nullptr;
	for (int i = 0 ; i<inst_qtwi->childCount() ; ++i) {
		auto child_loc = inst_qtwi->child(i);
		if (child_loc->data(0, Qt::DisplayRole).toString() == location_str) {
			loc_qtwi = child_loc;
			break;
		}
	}
	if (!loc_qtwi) {
		QStringList loc_strl{location_str};
		loc_qtwi = new QTreeWidgetItem(inst_qtwi, loc_strl, TerminalStripTreeWidget::Location);
	}

		//Add the terminal strip
	QStringList name{terminal_strip->name()};
	auto strip_item = new QTreeWidgetItem(loc_qtwi, name, TerminalStripTreeWidget::Strip);
	strip_item->setData(0, TerminalStripTreeWidget::UUID_USER_ROLE, terminal_strip->uuid());
	strip_item->setIcon(0, QET::Icons::TerminalStrip);

		//Add child terminal of the strip
	for (auto i=0 ; i<terminal_strip->physicalTerminalCount() ; ++i)
	{
		auto ptd = terminal_strip->physicalTerminalData(i);
		if (ptd.real_terminals_vector.size())
		{
			auto real_t = ptd.real_terminals_vector.first();
			auto terminal_item = new QTreeWidgetItem(strip_item, QStringList(real_t.label_), TerminalStripTreeWidget::Terminal);
			terminal_item->setData(0, TerminalStripTreeWidget::UUID_USER_ROLE, real_t.element_uuid.toString());
			terminal_item->setIcon(0, QET::Icons::ElementTerminal);

			if (real_t.element_) {
				m_uuid_terminal_H.insert(real_t.element_uuid, qgraphicsitem_cast<TerminalElement *>(real_t.element_));
			}
		}
	}

	m_item_strip_H.insert(strip_item, terminal_strip);
	m_uuid_strip_H.insert(terminal_strip->uuid(), terminal_strip);
	return strip_item;
}

/**
 * @brief TerminalStripEditor::addFreeTerminal
 * Add free terminal (aka terminal which not belong to a terminal strip)
 * in the tree widget
 */
void TerminalStripEditor::addFreeTerminal()
{
	ElementProvider ep(m_project);
	auto vector_ = ep.freeTerminal();

	if (vector_.isEmpty()) {
		return;
	}

		//Sort the terminal element by label
	std::sort(vector_.begin(), vector_.end(), [](TerminalElement *a, TerminalElement *b) {
		return a->actualLabel() < b->actualLabel();
	});

	auto free_terminal_item = ui->m_terminal_strip_tw->topLevelItem(1);

	for (const auto terminal : qAsConst(vector_))
	{
		QUuid uuid_ = terminal->uuid();
		QStringList strl{terminal->actualLabel()};
		auto item = new QTreeWidgetItem(free_terminal_item, strl, TerminalStripTreeWidget::Terminal);
		item->setData(0, TerminalStripTreeWidget::UUID_USER_ROLE, uuid_.toString());
		item->setIcon(0, QET::Icons::ElementTerminal);

		m_uuid_terminal_H.insert(uuid_, terminal);
	}
}

/**
 * @brief TerminalStripEditor::setCurrentStrip
 * Set the current terminal strip edited to \p strip_
 * @param strip_
 */
void TerminalStripEditor::setCurrentStrip(TerminalStrip *strip_)
{
	if (strip_ == m_current_strip) {
		return;
	}

	if (m_current_strip) {
		disconnect(m_current_strip, &TerminalStrip::orderChanged, this, &TerminalStripEditor::on_m_reload_pb_clicked);
	}

	if (!strip_)
	{
		ui->m_installation_le ->clear();
		ui->m_location_le     ->clear();
		ui->m_name_le         ->clear();
		ui->m_comment_le      ->clear();
		ui->m_description_te  ->clear();
		m_current_strip = nullptr;

		ui->m_table_widget->setModel(nullptr);
		if (m_model) {
			m_model->deleteLater();
			m_model = nullptr;
		}
	}
	else
	{
		ui->m_installation_le ->setText(strip_->installation());
		ui->m_location_le     ->setText(strip_->location());
		ui->m_name_le         ->setText(strip_->name());
		ui->m_comment_le      ->setText(strip_->comment());
		ui->m_description_te  ->setPlainText(strip_->description());
		m_current_strip = strip_;

		if (m_model)
			m_model->deleteLater();

		m_model = new TerminalStripModel(strip_, this);
		ui->m_table_widget->setModel(m_model);
		spanMultiLevelTerminals();
		selectionChanged();	//Used to update child widgets

		connect(m_current_strip, &TerminalStrip::orderChanged, this, &TerminalStripEditor::on_m_reload_pb_clicked);
		connect(ui->m_table_widget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TerminalStripEditor::selectionChanged);
	}
}

/**
 * @brief TerminalStripEditor::spanMultiLevelTerminals
 * Span row of m_table_widget for multi-level terminal
 */
void TerminalStripEditor::spanMultiLevelTerminals()
{
	if (!m_current_strip) {
		return;
	}

	ui->m_table_widget->clearSpans();
	auto current_row = 0;
	for (auto i = 0 ; i < m_current_strip->physicalTerminalCount() ; ++i)
	{
		const auto level_count = m_current_strip->physicalTerminalData(i).real_terminals_vector.size();
		if (level_count > 1) {
			ui->m_table_widget->setSpan(current_row, 0, level_count, 1);
		}
		current_row += level_count;
	}
}

/**
 * @brief TerminalStripEditor::selectionChanged
 * Update the editor according to the current selection
 */
void TerminalStripEditor::selectionChanged()
{
	if (!m_model) {
		ui->m_auto_ordering_pb  ->setDisabled(true);
		ui->m_group_terminals_pb->setDisabled(true);
		ui->m_ungroup_pb        ->setDisabled(true);
		ui->m_level_sb          ->setDisabled(true);
		ui->m_type_cb           ->setDisabled(true);
		ui->m_function_cb       ->setDisabled(true);
		ui->m_led_cb            ->setDisabled(true);
		return;
	}

	ui->m_auto_ordering_pb->setEnabled(true);

	const auto index_list = ui->m_table_widget->selectionModel()->selectedIndexes();

	if (index_list.isEmpty()) {
		ui->m_type_cb     ->setDisabled(true);
		ui->m_function_cb ->setDisabled(true);
		ui->m_led_cb      ->setDisabled(true);
	} else {
		ui->m_type_cb     ->setEnabled(true);
		ui->m_function_cb ->setEnabled(true);
		ui->m_led_cb      ->setEnabled(true);
	}

	const auto terminal_vector = m_model->physicalTerminalDataForIndex(index_list);

		//Enable/disable group button
	ui->m_group_terminals_pb->setEnabled(terminal_vector.size() > 1 ? true : false);

		//Enable/disable ungroup button
	auto it_= std::find_if(terminal_vector.constBegin(), terminal_vector.constEnd(), [](const PhysicalTerminalData &data)
	{
		if (data.real_terminals_vector.size() >= 2) {
			return true;
		} else {
			return false;
		}
	});
	ui->m_ungroup_pb->setDisabled(it_ == terminal_vector.constEnd());

		//Enable/disable level spinbox
	bool enable_ = false;
	for (const auto &physical : terminal_vector)
	{
		if (physical.real_terminals_vector.size() > 1) {
			enable_ = true;
			break;
		}
	}
	ui->m_level_sb->setEnabled(enable_);
}

/**
 * @brief TerminalStripEditor::on_m_add_terminal_strip_pb_clicked
 * Action when user click on add terminal strip button
 */
void TerminalStripEditor::on_m_add_terminal_strip_pb_clicked()
{
	QScopedPointer<TerminalStripCreatorDialog> dialog(new TerminalStripCreatorDialog(m_project, this));

	if (auto item = ui->m_terminal_strip_tw->currentItem())
	{
		if (item->type() == TerminalStripTreeWidget::Strip) {
			item = item->parent();
		}
		if (item->type() == TerminalStripTreeWidget::Location) {
			dialog->setLocation(item->data(0, Qt::DisplayRole).toString());
			item = item->parent();
		}
		if (item->type() == TerminalStripTreeWidget::Installation) {
			dialog->setInstallation(item->data(0, Qt::DisplayRole).toString());
		}
	}

	if (dialog->exec() == QDialog::Accepted)
	{
		auto ts = dialog->generatedTerminalStrip();
		m_project->undoStack()->push(new AddTerminalStripCommand(ts, m_project));

		auto item = addTerminalStrip(ts);
		ui->m_terminal_strip_tw->setCurrentItem(item);
	}
}

/**
 * @brief TerminalStripEditor::on_m_remove_terminal_strip_pb_clicked
 * Action when user click on remove terminal strip button
 */
void TerminalStripEditor::on_m_remove_terminal_strip_pb_clicked()
{
	auto item = ui->m_terminal_strip_tw->currentItem();
	if (auto strip = m_item_strip_H.value(item))
	{
		m_item_strip_H.remove(item);
		m_uuid_strip_H.remove(strip->uuid());
		delete item;

		m_project->undoStack()->push(new RemoveTerminalStripCommand(strip, m_project));
	}

	on_m_reload_pb_clicked();
}

/**
 * @brief TerminalStripEditor::on_m_reload_pb_clicked
 */
void TerminalStripEditor::on_m_reload_pb_clicked()
{
   auto current_ = m_current_strip;

   ui->m_terminal_strip_tw->clear();
   m_item_strip_H.clear();
   m_uuid_terminal_H.clear();
   m_uuid_strip_H.clear();

   qDeleteAll(m_item_strip_H.keyBegin(), m_item_strip_H.keyEnd());

   buildTree();
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	ui->m_terminal_strip_tw->expandRecursively(ui->m_terminal_strip_tw->rootIndex());
#else
	ui->m_terminal_strip_tw->expandAll();
#endif

	//Reselect the tree widget item of the current edited strip
   auto item = m_item_strip_H.key(current_);
   if (item) {
	   ui->m_terminal_strip_tw->setCurrentItem(item);
   }
}

/**
 * @brief TerminalStripEditor::on_m_terminal_strip_tw_currentItemChanged
 * @param current
 * @param previous
 */
void TerminalStripEditor::on_m_terminal_strip_tw_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
	Q_UNUSED(previous)

	if (!current) {
		setCurrentStrip(nullptr);
		ui->m_remove_terminal_strip_pb->setDisabled(true);
		return;
	}

	TerminalStrip *strip_ = nullptr;
	if (current->type() == TerminalStripTreeWidget::Strip) {
		strip_ = m_item_strip_H.value(current);
		ui->m_remove_terminal_strip_pb->setEnabled(true);
	}
	else if (current->type() == TerminalStripTreeWidget::Terminal
			 && current->parent()
			 && current->parent()->type() == TerminalStripTreeWidget::Strip) {
		strip_ = m_item_strip_H.value(current->parent());
		ui->m_remove_terminal_strip_pb->setDisabled(true);
	} else {
		ui->m_remove_terminal_strip_pb->setDisabled(true);
	}

	setCurrentStrip(strip_);
}

void TerminalStripEditor::on_m_dialog_button_box_clicked(QAbstractButton *button)
{
	Q_UNUSED(button)

	auto role = ui->m_dialog_button_box->buttonRole(button);

	if (role == QDialogButtonBox::ApplyRole)
	{
		if (m_current_strip)
		{
			m_project->undoStack()->beginMacro(tr("Modifier des propriétés de borniers"));

			TerminalStripData data;
			data.m_installation = ui->m_installation_le->text();
			data.m_location     = ui->m_location_le->text();
			data.m_name         = ui->m_name_le->text();
			data.m_comment      = ui->m_comment_le->text();
			data.m_description  = ui->m_description_te->toPlainText();

			m_project->undoStack()->push(new ChangeTerminalStripData(m_current_strip, data, nullptr));

			if (m_model)
			{
				for (const auto &data_ : m_model->modifiedRealTerminalData())
				{
					auto original_ = data_.first;
					auto edited_   = data_.second;
					auto element   = original_.element_;
					if (element) {
						auto current_data = element->elementData();
						current_data.setTerminalType(edited_.type_);
						current_data.setTerminalFunction(edited_.function_);
						current_data.setTerminalLED(edited_.led_);
						current_data.m_informations.addValue(QStringLiteral("label"), edited_.label_);

						if (element->elementData() != current_data)
							m_project->undoStack()->push(new ChangeElementDataCommand(element, current_data));
						if (edited_.level_)
							m_project->undoStack()->push(new ChangeTerminalLevel(m_current_strip, original_, edited_.level_));
					}
				}
			}

			m_project->undoStack()->endMacro();
		}
	}

	on_m_reload_pb_clicked();
}

/**
 * @brief TerminalStripEditor::on_m_auto_pos_pb_clicked
 */
void TerminalStripEditor::on_m_auto_ordering_pb_clicked()
{
	if (m_project && m_current_strip) {
		m_project->undoStack()->push(new SortTerminalStripCommand(m_current_strip));
	}
}

/**
 * @brief TerminalStripEditor::on_m_group_terminals_pb_clicked
 */
void TerminalStripEditor::on_m_group_terminals_pb_clicked()
{
	if (m_model && m_current_strip && m_project)
	{
		auto rtd_vector = m_model->realTerminalDataForIndex(ui->m_table_widget->selectionModel()->selectedIndexes());
		if (rtd_vector.size() >= 2)
		{
			auto receiver_ = m_current_strip->physicalTerminalData(rtd_vector.takeFirst());
			m_project->undoStack()->push(new GroupTerminalsCommand(m_current_strip, receiver_, rtd_vector));
		}
	}
}

/**
 * @brief TerminalStripEditor::on_m_ungroup_pb_clicked
 */
void TerminalStripEditor::on_m_ungroup_pb_clicked()
{
	if (m_model && m_current_strip)
	{
		const auto rtd_vector = m_model->realTerminalDataForIndex(ui->m_table_widget->selectionModel()->selectedIndexes());
		m_project->undoStack()->push(new UnGroupTerminalsCommand(m_current_strip, rtd_vector));
	}
}

/**
 * @brief TerminalStripEditor::on_m_level_sb_valueChanged
 * @param arg1
 */
void TerminalStripEditor::on_m_level_sb_valueChanged(int arg1)
{
	if (m_model)
	{
		const auto index_list = ui->m_table_widget->selectionModel()->selectedIndexes();

		for (auto index : index_list)
		{
			auto level_index = m_model->index(index.row(), 1, index.parent());
			if (level_index.isValid())
			{
				m_model->setData(level_index, arg1);
			}
		}
	}
}

void TerminalStripEditor::on_m_type_cb_activated(int index)
{
	if (m_model)
	{
		const auto index_list = ui->m_table_widget->selectionModel()->selectedIndexes();

		for (auto model_index : index_list)
		{
			auto type_index = m_model->index(model_index.row(), 6, model_index.parent());
			if (type_index.isValid())
			{
				ElementData::TerminalType override_type;
				switch (index) {
					case 0:
						override_type = ElementData::TTGeneric; break;
					case 1:
						override_type = ElementData::TTFuse; break;
					case 2:
						override_type = ElementData::TTSectional; break;
					case 3:
						override_type = ElementData::TTDiode; break;
					case 4:
						override_type = ElementData::TTGround; break;
					default:
						override_type = ElementData::TTGeneric; break;
				}
				m_model->setData(type_index, override_type);
			}
		}
	}
}


void TerminalStripEditor::on_m_function_cb_activated(int index)
{
	if (m_model)
	{
		const auto index_list = ui->m_table_widget->selectionModel()->selectedIndexes();

		for (auto model_index : index_list)
		{
			auto function_index = m_model->index(model_index.row(), 7, model_index.parent());
			if (function_index.isValid())
			{
				ElementData::TerminalFunction override_function;
				switch (index) {
					case 0:
						override_function = ElementData::TFGeneric; break;
					case 1:
						override_function = ElementData::TFPhase; break;
					case 2:
						override_function = ElementData::TFNeutral; break;
					default:
						override_function = ElementData::TFGeneric; break;
				}
				m_model->setData(function_index, override_function);
			}
		}
	}
}


void TerminalStripEditor::on_m_led_cb_activated(int index)
{
	if (m_model)
	{
		const auto index_list = ui->m_table_widget->selectionModel()->selectedIndexes();

		for (auto model_index : index_list)
		{
			auto led_index = m_model->index(model_index.row(), 8, model_index.parent());

			if (led_index.isValid()) {
				m_model->setData(led_index,
								 index == 0 ? false : true);
			}
		}
	}
}

