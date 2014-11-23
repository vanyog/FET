/***************************************************************************
                          modifyconstraintteacherhomeroomsform.h  -  description
                             -------------------
    begin                : April 8, 2005
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

#ifndef MODIFYCONSTRAINTTEACHERHOMEROOMSFORM_H
#define MODIFYCONSTRAINTTEACHERHOMEROOMSFORM_H

#include "ui_modifyconstraintteacherhomeroomsform_template.h"
#include "timetable_defs.h"
#include "timetable.h"
#include "fet.h"

class ModifyConstraintTeacherHomeRoomsForm : public QDialog, Ui::ModifyConstraintTeacherHomeRoomsForm_template  {
	Q_OBJECT
public:
	ModifyConstraintTeacherHomeRoomsForm(QWidget* parent, ConstraintTeacherHomeRooms* ctr);
	~ModifyConstraintTeacherHomeRoomsForm();

	void updateRoomsListWidget();

public slots:
	void addRoom();
	void removeRoom();

	void ok();
	void cancel();
	
	void clear();
private:
	ConstraintTeacherHomeRooms* _ctr;
};

#endif
