/***************************************************************************
                          addconstraintactivitiessamestartinghourform.cpp  -  description
                             -------------------
    begin                : Feb 15, 2005
    copyright            : (C) 2005 by Lalescu Liviu
    email                : Please see http://lalescu.ro/liviu/ for details about contacting Liviu Lalescu (in particular, you can find here the e-mail address)
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QMessageBox>

#include "longtextmessagebox.h"

#include "addconstraintactivitiessamestartinghourform.h"

#include <QListWidget>
#include <QAbstractItemView>
#include <QScrollBar>

AddConstraintActivitiesSameStartingHourForm::AddConstraintActivitiesSameStartingHourForm(QWidget* parent): QDialog(parent)
{
	setupUi(this);

	addConstraintPushButton->setDefault(true);
	
	activitiesListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	selectedActivitiesListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

	connect(closePushButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(addConstraintPushButton, SIGNAL(clicked()), this, SLOT(addConstraint()));
	connect(activitiesListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(addActivity()));
	connect(addAllActivitiesPushButton, SIGNAL(clicked()), this, SLOT(addAllActivities()));
	connect(selectedActivitiesListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(removeActivity()));
	connect(teachersComboBox, SIGNAL(activated(QString)), this, SLOT(filterChanged()));
	connect(studentsComboBox, SIGNAL(activated(QString)), this, SLOT(filterChanged()));
	connect(subjectsComboBox, SIGNAL(activated(QString)), this, SLOT(filterChanged()));
	connect(activityTagsComboBox, SIGNAL(activated(QString)), this, SLOT(filterChanged()));

	connect(clearPushButton, SIGNAL(clicked()), this, SLOT(clear()));

	centerWidgetOnScreen(this);
	restoreFETDialogGeometry(this);
	
	QSize tmp1=teachersComboBox->minimumSizeHint();
	Q_UNUSED(tmp1);
	QSize tmp2=studentsComboBox->minimumSizeHint();
	Q_UNUSED(tmp2);
	QSize tmp3=subjectsComboBox->minimumSizeHint();
	Q_UNUSED(tmp3);
	QSize tmp4=activityTagsComboBox->minimumSizeHint();
	Q_UNUSED(tmp4);
	
	teachersComboBox->addItem("");
	for(int i=0; i<gt.rules.teachersList.size(); i++){
		Teacher* tch=gt.rules.teachersList[i];
		teachersComboBox->addItem(tch->name);
	}
	teachersComboBox->setCurrentIndex(0);

	subjectsComboBox->addItem("");
	for(int i=0; i<gt.rules.subjectsList.size(); i++){
		Subject* sb=gt.rules.subjectsList[i];
		subjectsComboBox->addItem(sb->name);
	}
	subjectsComboBox->setCurrentIndex(0);

	activityTagsComboBox->addItem("");
	for(int i=0; i<gt.rules.activityTagsList.size(); i++){
		ActivityTag* st=gt.rules.activityTagsList[i];
		activityTagsComboBox->addItem(st->name);
	}
	activityTagsComboBox->setCurrentIndex(0);

	studentsComboBox->addItem("");
	for(int i=0; i<gt.rules.yearsList.size(); i++){
		StudentsYear* sty=gt.rules.yearsList[i];
		studentsComboBox->addItem(sty->name);
		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];
			studentsComboBox->addItem(stg->name);
			for(int k=0; k<stg->subgroupsList.size(); k++){
				StudentsSubgroup* sts=stg->subgroupsList[k];
				studentsComboBox->addItem(sts->name);
			}
		}
	}
	studentsComboBox->setCurrentIndex(0);

	selectedActivitiesListWidget->clear();
	this->selectedActivitiesList.clear();

	updateActivitiesListWidget();
}

AddConstraintActivitiesSameStartingHourForm::~AddConstraintActivitiesSameStartingHourForm()
{
	saveFETDialogGeometry(this);
}

bool AddConstraintActivitiesSameStartingHourForm::filterOk(Activity* act)
{
	QString tn=teachersComboBox->currentText();
	QString stn=studentsComboBox->currentText();
	QString sbn=subjectsComboBox->currentText();
	QString sbtn=activityTagsComboBox->currentText();
	int ok=true;

	//teacher
	if(tn!=""){
		bool ok2=false;
		for(QStringList::Iterator it=act->teachersNames.begin(); it!=act->teachersNames.end(); it++)
			if(*it == tn){
				ok2=true;
				break;
			}
		if(!ok2)
			ok=false;
	}

	//subject
	if(sbn!="" && sbn!=act->subjectName)
		ok=false;
		
	//activity tag
	if(sbtn!="" && !act->activityTagsNames.contains(sbtn))
		ok=false;
		
	//students
	if(stn!=""){
		bool ok2=false;
		for(QStringList::Iterator it=act->studentsNames.begin(); it!=act->studentsNames.end(); it++)
			if(*it == stn){
				ok2=true;
				break;
			}
		if(!ok2)
			ok=false;
	}
	
	return ok;
}

void AddConstraintActivitiesSameStartingHourForm::filterChanged()
{
	this->updateActivitiesListWidget();
}

void AddConstraintActivitiesSameStartingHourForm::updateActivitiesListWidget()
{
	activitiesListWidget->clear();

	this->activitiesList.clear();

	for(int i=0; i<gt.rules.activitiesList.size(); i++){
		Activity* ac=gt.rules.activitiesList[i];
		if(filterOk(ac)){
			activitiesListWidget->addItem(ac->getDescription(gt.rules));
			this->activitiesList.append(ac->id);
		}
	}
	
	int q=activitiesListWidget->verticalScrollBar()->minimum();
	activitiesListWidget->verticalScrollBar()->setValue(q);
}

void AddConstraintActivitiesSameStartingHourForm::addConstraint()
{
	TimeConstraint *ctr=NULL;

	double weight;
	QString tmp=weightLineEdit->text();
	weight_sscanf(tmp, "%lf", &weight);
	if(weight<0.0 || weight>100.0){
		QMessageBox::warning(this, tr("FET information"),
			tr("Invalid weight (percentage)"));
		return;
	}

	if(this->selectedActivitiesList.count()==0){
		QMessageBox::warning(this, tr("FET information"),
			tr("Empty list of selected activities"));
		return;
	}
	if(this->selectedActivitiesList.count()==1){
		QMessageBox::warning(this, tr("FET information"),
			tr("Only one selected activity"));
		return;
	}

	QList<int> ids;
	int i;
	QList<int>::iterator it;
	ids.clear();
	for(i=0, it=this->selectedActivitiesList.begin(); it!=this->selectedActivitiesList.end(); it++, i++){
		ids.append(*it);
	}
	
	ctr=new ConstraintActivitiesSameStartingHour(weight, this->selectedActivitiesList.count(), ids);
	bool tmp2=gt.rules.addTimeConstraint(ctr);
	
	if(tmp2){
		QString s=tr("Constraint added:");
		s+="\n\n";
		s+=ctr->getDetailedDescription(gt.rules);
		LongTextMessageBox::information(this, tr("FET information"), s);
	}
	else{
		QMessageBox::warning(this, tr("FET information"),
			tr("Constraint NOT added - please report error"));
		delete ctr;
	}
}

void AddConstraintActivitiesSameStartingHourForm::addActivity()
{
	if(activitiesListWidget->currentRow()<0)
		return;
	int tmp=activitiesListWidget->currentRow();
	int _id=this->activitiesList.at(tmp);
	
	QString actName=activitiesListWidget->currentItem()->text();
	assert(actName!="");
	int i;
	//duplicate?
	for(i=0; i<selectedActivitiesListWidget->count(); i++)
		if(actName==selectedActivitiesListWidget->item(i)->text())
			break;
	if(i<selectedActivitiesListWidget->count())
		return;
	selectedActivitiesListWidget->addItem(actName);
	selectedActivitiesListWidget->setCurrentRow(selectedActivitiesListWidget->count()-1);
	
	this->selectedActivitiesList.append(_id);
}

void AddConstraintActivitiesSameStartingHourForm::addAllActivities()
{
	for(int tmp=0; tmp<activitiesListWidget->count(); tmp++){
		int _id=this->activitiesList.at(tmp);
	
		QString actName=activitiesListWidget->item(tmp)->text();
		assert(actName!="");
		int i;
		//duplicate?
		for(i=0; i<selectedActivitiesList.count(); i++)
			if(selectedActivitiesList.at(i)==_id)
				break;
		if(i<selectedActivitiesList.count())
			continue;
			
		selectedActivitiesListWidget->addItem(actName);
		this->selectedActivitiesList.append(_id);
	}
	
	selectedActivitiesListWidget->setCurrentRow(selectedActivitiesListWidget->count()-1);
}

void AddConstraintActivitiesSameStartingHourForm::removeActivity()
{
	if(selectedActivitiesListWidget->currentRow()<0 || selectedActivitiesListWidget->count()<=0)
		return;
	int tmp=selectedActivitiesListWidget->currentRow();
	
	this->selectedActivitiesList.removeAt(tmp);
	
	selectedActivitiesListWidget->setCurrentRow(-1);
	QListWidgetItem* item=selectedActivitiesListWidget->takeItem(tmp);
	delete item;
	if(tmp<selectedActivitiesListWidget->count())
		selectedActivitiesListWidget->setCurrentRow(tmp);
	else
		selectedActivitiesListWidget->setCurrentRow(selectedActivitiesListWidget->count()-1);
}

void AddConstraintActivitiesSameStartingHourForm::clear()
{
	selectedActivitiesListWidget->clear();
	selectedActivitiesList.clear();
}
