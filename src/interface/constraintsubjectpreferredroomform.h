/***************************************************************************
                          constraintsubjectpreferredroomform.h  -  description
                             -------------------
    begin                : Apr 8, 2005
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

#ifndef CONSTRAINTSUBJECTPREFERREDROOMFORM_H
#define CONSTRAINTSUBJECTPREFERREDROOMFORM_H

#include "ui_constraintsubjectpreferredroomform_template.h"
#include "timetable_defs.h"
#include "timetable.h"
#include "fet.h"

class ConstraintSubjectPreferredRoomForm : public QDialog, Ui::ConstraintSubjectPreferredRoomForm_template  {
	Q_OBJECT
public:
	SpaceConstraintsList visibleConstraintsList;

	ConstraintSubjectPreferredRoomForm(QWidget* parent);
	~ConstraintSubjectPreferredRoomForm();

	bool filterOk(SpaceConstraint* ctr);

public slots:
	void constraintChanged(int index);
	void addConstraint();
	void modifyConstraint();
	void removeConstraint();

	void filterChanged();
};

#endif
