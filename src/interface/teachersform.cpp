//
//
// Description: This file is part of FET
//
//
// Author: Lalescu Liviu <Please see http://lalescu.ro/liviu/ for details about contacting Liviu Lalescu (in particular, you can find here the e-mail address)>
// Copyright (C) 2003 Liviu Lalescu <http://lalescu.ro/liviu/>
//
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "timetable_defs.h"
#include "timetable.h"
#include "fet.h"
#include "teachersform.h"
#include "teacher.h"

#include <QInputDialog>

#include <QMessageBox>

#include <QListWidget>
#include <QAbstractItemView>

#include <QSplitter>
#include <QSettings>
#include <QObject>
#include <QMetaObject>

extern const QString COMPANY;
extern const QString PROGRAM;

TeachersForm::TeachersForm(QWidget* parent): QDialog(parent)
{
	setupUi(this);
	
	currentTeacherTextEdit->setReadOnly(true);

	renameTeacherPushButton->setDefault(true);

	teachersListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

	connect(renameTeacherPushButton, SIGNAL(clicked()), this, SLOT(renameTeacher()));
	connect(closePushButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(addTeacherPushButton, SIGNAL(clicked()), this, SLOT(addTeacher()));
	connect(sortTeachersPushButton, SIGNAL(clicked()), this, SLOT(sortTeachers()));
	connect(removeTeacherPushButton, SIGNAL(clicked()), this, SLOT(removeTeacher()));
	connect(teachersListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(teacherChanged(int)));
	connect(activateTeacherPushButton, SIGNAL(clicked()), this, SLOT(activateTeacher()));
	connect(deactivateTeacherPushButton, SIGNAL(clicked()), this, SLOT(deactivateTeacher()));
	connect(teachersListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(renameTeacher()));

	centerWidgetOnScreen(this);
	restoreFETDialogGeometry(this);
	//restore splitter state
	QSettings settings(COMPANY, PROGRAM);
	if(settings.contains(this->metaObject()->className()+QString("/splitter-state")))
		splitter->restoreState(settings.value(this->metaObject()->className()+QString("/splitter-state")).toByteArray());
	
	teachersListWidget->clear();
	for(int i=0; i<gt.rules.teachersList.size(); i++){
		Teacher* tch=gt.rules.teachersList[i];
		teachersListWidget->addItem(tch->name);
	}
	
	if(teachersListWidget->count()>0)
		teachersListWidget->setCurrentRow(0);
}


TeachersForm::~TeachersForm()
{
	saveFETDialogGeometry(this);
	//save splitter state
	QSettings settings(COMPANY, PROGRAM);
	settings.setValue(this->metaObject()->className()+QString("/splitter-state"), splitter->saveState());
}

void TeachersForm::addTeacher()
{
	bool ok = false;
	Teacher* tch=new Teacher();
	tch->name = QInputDialog::getText( this, tr("Add teacher"), tr("Please enter teacher's name") ,
	 QLineEdit::Normal, QString(), &ok );

	if ( ok && !((tch->name).isEmpty()) ){
		// user entered something and pressed OK
		if(!gt.rules.addTeacher(tch)){
			QMessageBox::information( this, tr("Teacher insertion dialog"),
				tr("Could not insert item. Must be a duplicate"));
			delete tch;
		}
		else{
			teachersListWidget->addItem(tch->name);
			teachersListWidget->setCurrentRow(teachersListWidget->count()-1);
		}
	}
	else{
		if(ok){ //the user entered nothing
			QMessageBox::information(this, tr("FET information"), tr("Incorrect name"));
		}
		delete tch;// user entered nothing or pressed Cancel
	}
}

void TeachersForm::removeTeacher()
{
	int i=teachersListWidget->currentRow();
	if(teachersListWidget->currentRow()<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	QString text=teachersListWidget->currentItem()->text();
	int teacher_ID=gt.rules.searchTeacher(text);
	if(teacher_ID<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	if(QMessageBox::warning( this, tr("FET"),
		tr("Are you sure you want to delete this teacher and all related activities and constraints?"),
		tr("Yes"), tr("No"), 0, 0, 1 ) == 1)
		return;

	int tmp=gt.rules.removeTeacher(text);
	if(tmp){
		teachersListWidget->setCurrentRow(-1);
		QListWidgetItem* item;
		item=teachersListWidget->takeItem(i);
		delete item;

		if(i>=teachersListWidget->count())
			i=teachersListWidget->count()-1;
		if(i>=0)
			teachersListWidget->setCurrentRow(i);
		else
			currentTeacherTextEdit->setPlainText(QString(""));
	}
}

void TeachersForm::renameTeacher()
{
	int i=teachersListWidget->currentRow();
	if(teachersListWidget->currentRow()<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	QString initialTeacherName=teachersListWidget->currentItem()->text();
	int teacher_ID=gt.rules.searchTeacher(initialTeacherName);
	if(teacher_ID<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	bool ok = false;
	QString finalTeacherName;
	finalTeacherName = QInputDialog::getText( this, tr("Modify teacher"), tr("Please enter new teacher's name") ,
	 QLineEdit::Normal, initialTeacherName, &ok);

	if ( ok && !(finalTeacherName.isEmpty())){
		// user entered something and pressed OK
		if(gt.rules.searchTeacher(finalTeacherName)>=0){
			QMessageBox::information( this, tr("Teacher modification dialog"),
				tr("Could not modify item. New name must be a duplicate"));
		}
		else{
			gt.rules.modifyTeacher(initialTeacherName, finalTeacherName);
			teachersListWidget->item(i)->setText(finalTeacherName);
			teacherChanged(teachersListWidget->currentRow());
		}
	}
}

void TeachersForm::sortTeachers()
{
	gt.rules.sortTeachersAlphabetically();

	teachersListWidget->clear();
	for(int i=0; i<gt.rules.teachersList.size(); i++){
		Teacher* tch=gt.rules.teachersList[i];
		teachersListWidget->addItem(tch->name);
	}
	
	if(teachersListWidget->count()>0)
		teachersListWidget->setCurrentRow(0);
}

void TeachersForm::teacherChanged(int index)
{
	if(index<0){
		currentTeacherTextEdit->setPlainText("");
		return;
	}
	
	Teacher* t=gt.rules.teachersList.at(index);
	assert(t);
	QString s=t->getDetailedDescriptionWithConstraints(gt.rules);
	currentTeacherTextEdit->setPlainText(s);
}

void TeachersForm::activateTeacher()
{
	if(teachersListWidget->currentRow()<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	QString teacherName=teachersListWidget->currentItem()->text();
	int count=gt.rules.activateTeacher(teacherName);
	QMessageBox::information(this, tr("FET information"), tr("Activated a number of %1 activities").arg(count));
}

void TeachersForm::deactivateTeacher()
{
	if(teachersListWidget->currentRow()<0){
		QMessageBox::information(this, tr("FET information"), tr("Invalid selected teacher"));
		return;
	}

	QString teacherName=teachersListWidget->currentItem()->text();
	int count=gt.rules.deactivateTeacher(teacherName);
	QMessageBox::information(this, tr("FET information"), tr("De-activated a number of %1 activities").arg(count));
}
