/***************************************************************************
                          teacherstatisticsform.h  -  description
                             -------------------
    begin                : March 25, 2006
    copyright            : (C) 2006 by Lalescu Liviu
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

#ifndef TEACHERSSTATISTICSFORM_H
#define TEACHERSSTATISTICSFORM_H

#include "ui_teachersstatisticsform_template.h"

#include <QDialog>

class TeachersStatisticsForm : public QDialog, Ui::TeachersStatisticsForm_template  {
	Q_OBJECT

public:
	TeachersStatisticsForm(QWidget* parent);
	~TeachersStatisticsForm();
};

#endif
