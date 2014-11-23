/***************************************************************************
                          modifyconstraintstudentssetmingapsbetweenbuildingchangesform.h  -  description
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

#ifndef MODIFYCONSTRAINTSTUDENTSSETMINGAPSBETWEENBUILDINGCHANGESFORM_H
#define MODIFYCONSTRAINTSTUDENTSSETMINGAPSBETWEENBUILDINGCHANGESFORM_H

#include "ui_modifyconstraintstudentssetmingapsbetweenbuildingchangesform_template.h"
#include "timetable_defs.h"
#include "timetable.h"
#include "fet.h"

class ModifyConstraintStudentsSetMinGapsBetweenBuildingChangesForm : public QDialog, Ui::ModifyConstraintStudentsSetMinGapsBetweenBuildingChangesForm_template  {
	Q_OBJECT
public:
	ConstraintStudentsSetMinGapsBetweenBuildingChanges* _ctr;

	ModifyConstraintStudentsSetMinGapsBetweenBuildingChangesForm(QWidget* parent, ConstraintStudentsSetMinGapsBetweenBuildingChanges* ctr);
	~ModifyConstraintStudentsSetMinGapsBetweenBuildingChangesForm();

	void updateStudentsComboBox();

public slots:
	void constraintChanged();
	void ok();
	void cancel();
};

#endif
