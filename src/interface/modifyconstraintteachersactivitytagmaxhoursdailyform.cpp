/***************************************************************************
                          modifyconstraintteachersactivitytagmaxhoursdailyform.cpp  -  description
                             -------------------
    begin                : 2009
    copyright            : (C) 2009 by Lalescu Liviu
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

#include "modifyconstraintteachersactivitytagmaxhoursdailyform.h"
#include "timeconstraint.h"

ModifyConstraintTeachersActivityTagMaxHoursDailyForm::ModifyConstraintTeachersActivityTagMaxHoursDailyForm(QWidget* parent, ConstraintTeachersActivityTagMaxHoursDaily* ctr): QDialog(parent)
{
	setupUi(this);

	okPushButton->setDefault(true);

	connect(okPushButton, SIGNAL(clicked()), this, SLOT(ok()));
	connect(cancelPushButton, SIGNAL(clicked()), this, SLOT(cancel()));

	centerWidgetOnScreen(this);
	restoreFETDialogGeometry(this);

	QSize tmp4=activityTagsComboBox->minimumSizeHint();
	Q_UNUSED(tmp4);
	
	this->_ctr=ctr;
	
	weightLineEdit->setText(CustomFETString::number(ctr->weightPercentage));
	
	updateMaxHoursSpinBox();
	
	maxHoursSpinBox->setValue(ctr->maxHoursDaily);

	activityTagsComboBox->clear();
	int j=-1;
	for(int k=0; k<gt.rules.activityTagsList.count(); k++){
		ActivityTag* at=gt.rules.activityTagsList.at(k);
		activityTagsComboBox->addItem(at->name);
		if(at->name==this->_ctr->activityTagName)
			j=k;
	}
	assert(j>=0);
	activityTagsComboBox->setCurrentIndex(j);

	constraintChanged();
}

ModifyConstraintTeachersActivityTagMaxHoursDailyForm::~ModifyConstraintTeachersActivityTagMaxHoursDailyForm()
{
	saveFETDialogGeometry(this);
}

void ModifyConstraintTeachersActivityTagMaxHoursDailyForm::updateMaxHoursSpinBox(){
	maxHoursSpinBox->setMinimum(1);
	maxHoursSpinBox->setMaximum(gt.rules.nHoursPerDay);	
}

void ModifyConstraintTeachersActivityTagMaxHoursDailyForm::constraintChanged()
{
	//nothing
}

void ModifyConstraintTeachersActivityTagMaxHoursDailyForm::ok()
{
	double weight;
	QString tmp=weightLineEdit->text();
	weight_sscanf(tmp, "%lf", &weight);
	if(weight<0.0 || weight>100.0){
		QMessageBox::warning(this, tr("FET information"),
			tr("Invalid weight (percentage)"));
		return;
	}

	int max_hours=maxHoursSpinBox->value();

	QString activityTagName=activityTagsComboBox->currentText();
	int ati=gt.rules.searchActivityTag(activityTagName);
	if(ati<0){
		QMessageBox::warning(this, tr("FET warning"), tr("Invalid activity tag"));
		return;
	}

	this->_ctr->weightPercentage=weight;
	this->_ctr->maxHoursDaily=max_hours;
	this->_ctr->activityTagName=activityTagName;

	gt.rules.internalStructureComputed=false;
	setRulesModifiedAndOtherThings(&gt.rules);
	
	this->close();
}

void ModifyConstraintTeachersActivityTagMaxHoursDailyForm::cancel()
{
	this->close();
}
