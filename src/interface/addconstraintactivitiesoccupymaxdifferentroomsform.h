/***************************************************************************
                          addconstraintactivitiesoccupymaxdifferentroomsform.h  -  description
                             -------------------
    begin                : Apr 29, 2012
    copyright            : (C) 2012 by Lalescu Liviu
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

#ifndef ADDCONSTRAINTACTIVITIESOCCUPYMAXDIFFERENTROOMSFORM_H
#define ADDCONSTRAINTACTIVITIESOCCUPYMAXDIFFERENTROOMSFORM_H

#include "ui_addconstraintactivitiesoccupymaxdifferentroomsform_template.h"
#include "timetable_defs.h"
#include "timetable.h"
#include "fet.h"

#include <QList>

class AddConstraintActivitiesOccupyMaxDifferentRoomsForm : public QDialog, Ui::AddConstraintActivitiesOccupyMaxDifferentRoomsForm_template  {
	Q_OBJECT
	
public:
	AddConstraintActivitiesOccupyMaxDifferentRoomsForm(QWidget* parent);
	~AddConstraintActivitiesOccupyMaxDifferentRoomsForm();

	void updateActivitiesListWidget();
	
	bool filterOk(Activity* act);

public slots:
	void addActivity();
	void addAllActivities();
	void removeActivity();
	
	void filterChanged();
	
	void clear();

	void addCurrentConstraint();

private:
	//the id's of the activities listed in the activities list
	QList<int> activitiesList;
	//the id-s of the activities listed in the list of selected activities
	QList<int> selectedActivitiesList;
};

#endif
