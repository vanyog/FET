/***************************************************************************
                          modifyconstraintteachersmaxbuildingchangesperweekform.cpp  -  description
                             -------------------
    begin                : Feb 10, 2005
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

#include "modifyconstraintteachersmaxbuildingchangesperweekform.h"
#include "spaceconstraint.h"

ModifyConstraintTeachersMaxBuildingChangesPerWeekForm::ModifyConstraintTeachersMaxBuildingChangesPerWeekForm(QWidget* parent, ConstraintTeachersMaxBuildingChangesPerWeek* ctr): QDialog(parent)
{
	setupUi(this);

	okPushButton->setDefault(true);

	connect(okPushButton, SIGNAL(clicked()), this, SLOT(ok()));
	connect(cancelPushButton, SIGNAL(clicked()), this, SLOT(cancel()));

	centerWidgetOnScreen(this);
	restoreFETDialogGeometry(this);
	
	this->_ctr=ctr;
	
	weightLineEdit->setText(CustomFETString::number(ctr->weightPercentage));
	
	maxChangesSpinBox->setMinimum(0);
	maxChangesSpinBox->setMaximum(gt.rules.nDaysPerWeek*gt.rules.nHoursPerDay);
	maxChangesSpinBox->setValue(ctr->maxBuildingChangesPerWeek);
		
	constraintChanged();
}

ModifyConstraintTeachersMaxBuildingChangesPerWeekForm::~ModifyConstraintTeachersMaxBuildingChangesPerWeekForm()
{
	saveFETDialogGeometry(this);
}

void ModifyConstraintTeachersMaxBuildingChangesPerWeekForm::constraintChanged()
{
}

void ModifyConstraintTeachersMaxBuildingChangesPerWeekForm::ok()
{
	double weight;
	QString tmp=weightLineEdit->text();
	weight_sscanf(tmp, "%lf", &weight);
	if(weight<100.0 || weight>100.0){
		QMessageBox::warning(this, tr("FET information"),
			tr("Invalid weight (percentage). It has to be 100"));
		return;
	}

	this->_ctr->weightPercentage=weight;
	this->_ctr->maxBuildingChangesPerWeek=maxChangesSpinBox->value();

	gt.rules.internalStructureComputed=false;
	setRulesModifiedAndOtherThings(&gt.rules);
	
	this->close();
}

void ModifyConstraintTeachersMaxBuildingChangesPerWeekForm::cancel()
{
	this->close();
}
