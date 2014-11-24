/*
File rules.cpp
*/

/***************************************************************************
                          rules.cpp  -  description
                             -------------------
    begin                : 2002
    copyright            : (C) 2002 by Lalescu Liviu
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

#include "timetable_defs.h"
#include "rules.h"

#include <QDir>

#include <algorithm>
#include <iostream>
using namespace std;

#include <QTextStream>
#include <QFile>
#include <QFileInfo>

#include <QDate>
#include <QTime>
#include <QLocale>

#include <QString>

#include <QXmlStreamReader>

#include <QString>
#include <QTranslator>

#include <QtAlgorithms>

#include <QSet>
#include <QHash>

//#include <QApplication>
#ifndef FET_COMMAND_LINE
#include <QProgressDialog>
#endif

#include <QRegExp>

#include "messageboxes.h"

#include "lockunlock.h"

#include "centerwidgetonscreen.h"

//static bool toSkipTime[MAX_TIME_CONSTRAINTS];
//static bool toSkipSpace[MAX_SPACE_CONSTRAINTS];

//extern QApplication* pqapplication;

extern bool students_schedule_ready;
extern bool rooms_schedule_ready;
extern bool teachers_schedule_ready;


FakeString::FakeString()
{
}

void FakeString::operator=(const QString& other)
{
	Q_UNUSED(other);
}

void FakeString::operator=(const char* str)
{
	Q_UNUSED(str);
}

void FakeString::operator+=(const QString& other)
{
	Q_UNUSED(other);
}

void FakeString::operator+=(const char* str)
{
	Q_UNUSED(str);
}


void Rules::init() //initializes the rules (empty, but with default hours and days)
{
	//defaults
	this->nHoursPerDay=12;
	this->hoursOfTheDay[0]="08:00";
	this->hoursOfTheDay[1]="09:00";
	this->hoursOfTheDay[2]="10:00";
	this->hoursOfTheDay[3]="11:00";
	this->hoursOfTheDay[4]="12:00";
	this->hoursOfTheDay[5]="13:00";
	this->hoursOfTheDay[6]="14:00";
	this->hoursOfTheDay[7]="15:00";
	this->hoursOfTheDay[8]="16:00";
	this->hoursOfTheDay[9]="17:00";
	this->hoursOfTheDay[10]="18:00";
	this->hoursOfTheDay[11]="19:00";
	//this->hoursOfTheDay[12]="20:00";

	this->nDaysPerWeek=5;
	this->daysOfTheWeek[0] = tr("Monday");
	this->daysOfTheWeek[1] = tr("Tuesday");
	this->daysOfTheWeek[2] = tr("Wednesday");
	this->daysOfTheWeek[3] = tr("Thursday");
	this->daysOfTheWeek[4] = tr("Friday");
	
	this->institutionName=tr("Default institution");
	this->comments=tr("Default comments");

	activitiesPointerHash.clear();
	bctSet.clear();
	btSet.clear();
	bcsSet.clear();
	apstHash.clear();
	aprHash.clear();
	mdbaHash.clear();
	tnatHash.clear();
	ssnatHash.clear();
	
	this->initialized=true;
}

bool Rules::computeInternalStructure(QWidget* parent)
{
	//To fix a bug reported by Frans on forum, on 7 May 2010.
	//If user generates, then changes some activities (changes teachers of them), then tries to generate but FET cannot precompute in generate_pre.cpp,
	//then if user views the timetable, the timetable of a teacher contains activities of other teacher.
	//The bug appeared because it is possible to compute internal structure, so internal activities change the teacher, but the timetables remain the same,
	//with the same activities indexes.
	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	//The order is important - firstly the teachers, subjects, activity tags and students.
	//After that, the buildings.
	//After that, the rooms.
	//After that, the activities.
	//After that, the time constraints.
	//After that, the space constraints.

	if(this->teachersList.size()>MAX_TEACHERS){
		RulesImpossible::warning(parent, tr("FET information"),
		 tr("You have too many teachers. You need to increase the variable MAX_TEACHERS (which is currently %1).")
		 .arg(MAX_TEACHERS));
		return false;
	}
	
	//kill augmented students sets
	QList<StudentsYear*> ayears;
	QList<StudentsGroup*> agroups;
	QList<StudentsSubgroup*> asubgroups;
	foreach(StudentsYear* year, augmentedYearsList){
		if(!ayears.contains(year))
			ayears.append(year);
		foreach(StudentsGroup* group, year->groupsList){
			if(!agroups.contains(group))
				agroups.append(group);
			foreach(StudentsSubgroup* subgroup, group->subgroupsList){
				if(!asubgroups.contains(subgroup))
					asubgroups.append(subgroup);
			}
		}
	}
	foreach(StudentsYear* year, ayears){
		assert(year!=NULL);
		delete year;
	}
	foreach(StudentsGroup* group, agroups){
		assert(group!=NULL);
		delete group;
	}
	foreach(StudentsSubgroup* subgroup, asubgroups){
		assert(subgroup!=NULL);
		delete subgroup;
	}
	augmentedYearsList.clear();
	//////////////////
	
	//copy list of students sets into augmented list
	QHash<QString, StudentsSet*> augmentedHash;
	
	foreach(StudentsYear* y, yearsList){
		StudentsYear* ay=new StudentsYear();
		ay->name=y->name;
		ay->numberOfStudents=y->numberOfStudents;
		ay->groupsList.clear();
		augmentedYearsList << ay;
		
		assert(!augmentedHash.contains(ay->name));
		augmentedHash.insert(ay->name, ay);
		
		foreach(StudentsGroup* g, y->groupsList){
			if(augmentedHash.contains(g->name)){
				StudentsSet* tmpg=augmentedHash.value(g->name);
				assert(tmpg->type==STUDENTS_GROUP);
				ay->groupsList<<((StudentsGroup*)tmpg);
			}
			else{
				StudentsGroup* ag=new StudentsGroup();
				ag->name=g->name;
				ag->numberOfStudents=g->numberOfStudents;
				ag->subgroupsList.clear();
				ay->groupsList << ag;
				
				assert(!augmentedHash.contains(ag->name));
				augmentedHash.insert(ag->name, ag);
			
				foreach(StudentsSubgroup* s, g->subgroupsList){
					if(augmentedHash.contains(s->name)){
						StudentsSet* tmps=augmentedHash.value(s->name);
						assert(tmps->type==STUDENTS_SUBGROUP);
						ag->subgroupsList<<((StudentsSubgroup*)tmps);
					}
					else{
						StudentsSubgroup* as=new StudentsSubgroup();
						as->name=s->name;
						as->numberOfStudents=s->numberOfStudents;
						ag->subgroupsList << as;
						
						assert(!augmentedHash.contains(as->name));
						augmentedHash.insert(as->name, as);
					}
				}
			}
		}
	}

	/////////
	for(int i=0; i<this->augmentedYearsList.size(); i++){
		StudentsYear* sty=this->augmentedYearsList[i];

		//if this year has no groups, insert something to simulate the whole year
		if(sty->groupsList.count()==0){
			StudentsGroup* tmpGroup = new StudentsGroup();
			tmpGroup->name = sty->name+" "+tr("Automatic Group", "Please keep the translation short. It is used when a year contains no groups and an automatic group "
			 "is added in the year, in the timetable (when viewing the students timetable from FET and also in the html timetables for students groups or subgroups)"
			 ". In the empty year there will be added a group with name = yearName+a space character+your translation of 'Automatic Group'.");
			tmpGroup->numberOfStudents = sty->numberOfStudents;
			sty->groupsList << tmpGroup;
		}
		
		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];

			//if this group has no subgroups, insert something to simulate the whole group
			if(stg->subgroupsList.size()==0){
				StudentsSubgroup* tmpSubgroup = new StudentsSubgroup();
				tmpSubgroup->name = stg->name+" "+tr("Automatic Subgroup", "Please keep the translation short. It is used when a group contains no subgroups and an automatic subgroup "
				 "is added in the group, in the timetable (when viewing the students timetable from FET and also in the html timetables for students subgroups)"
				 ". In the empty group there will be added a subgroup with name = groupName+a space character+your translation of 'Automatic Subgroup'.");
				tmpSubgroup->numberOfStudents=stg->numberOfStudents;
				stg->subgroupsList << tmpSubgroup;
			}
		}
	}
	//////////
	
	QSet<StudentsGroup*> allGroupsSet;
	QSet<StudentsSubgroup*> allSubgroupsSet;
	QList<StudentsGroup*> allGroupsList;
	QList<StudentsSubgroup*> allSubgroupsList;
	
	for(int i=0; i<this->augmentedYearsList.size(); i++){
		StudentsYear* sty=this->augmentedYearsList.at(i);
		sty->indexInAugmentedYearsList=i;

		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList.at(j);
			if(!allGroupsSet.contains(stg)){
				allGroupsSet.insert(stg);
				allGroupsList.append(stg);
				stg->indexInInternalGroupsList=allGroupsSet.count()-1;
			}
			
			for(int k=0; k<stg->subgroupsList.size(); k++)
				if(!allSubgroupsSet.contains(stg->subgroupsList.at(k))){
					allSubgroupsSet.insert(stg->subgroupsList.at(k));
					allSubgroupsList.append(stg->subgroupsList.at(k));
					stg->subgroupsList.at(k)->indexInInternalSubgroupsList=allSubgroupsSet.count()-1;
				}
		}
	}
	int tmpNSubgroups=allSubgroupsList.count();
	if(tmpNSubgroups>MAX_TOTAL_SUBGROUPS){
		RulesImpossible::warning(parent, tr("FET information"),
		 tr("You have too many total subgroups. You need to increase the variable MAX_TOTAL_SUBGROUPS (which is currently %1).")
		 .arg(MAX_TOTAL_SUBGROUPS));
		return false;
	}
	this->internalSubgroupsList.resize(tmpNSubgroups);

	int counter=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList.at(i);
		if(act->active)
			counter++;
	}
	if(counter>MAX_ACTIVITIES){
		RulesImpossible::warning(parent, tr("FET information"),
		 tr("You have too many active activities. You need to increase the variable MAX_ACTIVITIES (which is currently %1).")
		 .arg(MAX_ACTIVITIES));
		return false;
	}

	if(this->buildingsList.size()>MAX_BUILDINGS){
		RulesImpossible::warning(parent, tr("FET information"),
		 tr("You have too many buildings. You need to increase the variable MAX_BUILDINGS (which is currently %1).")
		 .arg(MAX_BUILDINGS));
		return false;
	}
	
	if(this->roomsList.size()>MAX_ROOMS){
		RulesImpossible::warning(parent, tr("FET information"),
		 tr("You have too many rooms. You need to increase the variable MAX_ROOMS (which is currently %1).")
		 .arg(MAX_ROOMS));
		return false;
	}
	
	assert(this->initialized);

	//days and hours
	assert(this->nHoursPerDay>0);
	assert(this->nDaysPerWeek>0);
	this->nHoursPerWeek=this->nHoursPerDay*this->nDaysPerWeek;

	//teachers
	int i;
	Teacher* tch;
	this->nInternalTeachers=this->teachersList.size();
	assert(this->nInternalTeachers<=MAX_TEACHERS);
	this->internalTeachersList.resize(this->nInternalTeachers);
	for(i=0; i<this->teachersList.size(); i++){
		tch=teachersList[i];
		this->internalTeachersList[i]=tch;
	}
	assert(i==this->nInternalTeachers);

	teachersHash.clear();
	for(int i=0; i<nInternalTeachers; i++)
		teachersHash.insert(internalTeachersList[i]->name, i);

	//subjects
	Subject* sbj;
	this->nInternalSubjects=this->subjectsList.size();
	this->internalSubjectsList.resize(this->nInternalSubjects);
	for(i=0; i<this->subjectsList.size(); i++){
		sbj=this->subjectsList[i];
		this->internalSubjectsList[i]=sbj;
	}
	assert(i==this->nInternalSubjects);

	subjectsHash.clear();
	for(int i=0; i<nInternalSubjects; i++)
		subjectsHash.insert(internalSubjectsList[i]->name, i);

	//activity tags
	ActivityTag* at;
	this->nInternalActivityTags=this->activityTagsList.size();
	this->internalActivityTagsList.resize(this->nInternalActivityTags);
	for(i=0; i<this->activityTagsList.size(); i++){
		at=this->activityTagsList[i];
		this->internalActivityTagsList[i]=at;
	}
	assert(i==this->nInternalActivityTags);

	activityTagsHash.clear();
	for(int i=0; i<nInternalActivityTags; i++)
		activityTagsHash.insert(internalActivityTagsList[i]->name, i);

	//students
	this->nInternalSubgroups=0;
	for(int i=0; i<allSubgroupsList.count(); i++){
		assert(allSubgroupsList.at(i)->indexInInternalSubgroupsList==i);
		this->internalSubgroupsList[this->nInternalSubgroups]=allSubgroupsList.at(i);
		this->nInternalSubgroups++;
	}

	this->internalGroupsList.clear();
	for(int i=0; i<allGroupsList.count(); i++){
		assert(allGroupsList.at(i)->indexInInternalGroupsList==i);
		this->internalGroupsList.append(allGroupsList.at(i));
	}
	
	studentsHash.clear();
	foreach(StudentsYear* year, augmentedYearsList){
		studentsHash.insert(year->name, year);
		foreach(StudentsGroup* group, year->groupsList){
			studentsHash.insert(group->name, group);
			foreach(StudentsSubgroup* subgroup, group->subgroupsList)
				studentsHash.insert(subgroup->name, subgroup);
		}
	}
	
/*	for(int i=0; i<this->augmentedYearsList.size(); i++){
		StudentsYear* sty=this->augmentedYearsList[i];
		
		assert(sty->groupsList.count()>0);

		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];
			
			assert(stg->subgroupsList.count()>0);

			for(int k=0; k<stg->subgroupsList.size(); k++){
				StudentsSubgroup* sts=stg->subgroupsList[k];

				bool existing=false;
				for(int i=0; i<this->nInternalSubgroups; i++)
					if(this->internalSubgroupsList[i]->name==sts->name){
						existing=true;
						sts->indexInInternalSubgroupsList=i;
						break;
					}
				if(!existing){
					assert(this->nInternalSubgroups<MAX_TOTAL_SUBGROUPS);
					assert(this->nInternalSubgroups<tmpNSubgroups);
					sts->indexInInternalSubgroupsList=this->nInternalSubgroups;
					this->internalSubgroupsList[this->nInternalSubgroups++]=sts;
				}
			}
		}
	}*/
	assert(this->nInternalSubgroups==tmpNSubgroups);

	//buildings
	internalBuildingsList.resize(buildingsList.size());
	this->nInternalBuildings=0;
	assert(this->buildingsList.size()<=MAX_BUILDINGS);
	for(int i=0; i<this->buildingsList.size(); i++){
		Building* bu=this->buildingsList[i];
		bu->computeInternalStructure(*this);
	}
	
	for(int i=0; i<this->buildingsList.size(); i++){
		Building* bu=this->buildingsList[i];
		this->internalBuildingsList[this->nInternalBuildings++]=bu;
	}
	assert(this->nInternalBuildings==this->buildingsList.size());

	buildingsHash.clear();
	for(int i=0; i<nInternalBuildings; i++)
		buildingsHash.insert(internalBuildingsList[i]->name, i);

	//rooms
	internalRoomsList.resize(roomsList.size());
	this->nInternalRooms=0;
	assert(this->roomsList.size()<=MAX_ROOMS);
	for(int i=0; i<this->roomsList.size(); i++){
		Room* rm=this->roomsList[i];
		rm->computeInternalStructure(*this);
	}
	
	for(int i=0; i<this->roomsList.size(); i++){
		Room* rm=this->roomsList[i];
		this->internalRoomsList[this->nInternalRooms++]=rm;
	}
	assert(this->nInternalRooms==this->roomsList.size());

	roomsHash.clear();
	for(int i=0; i<nInternalRooms; i++)
		roomsHash.insert(internalRoomsList[i]->name, i);

	//activities
	int range=0;
	foreach(Activity* act, this->activitiesList)
		if(act->active)
			range++;
	QProgressDialog progress(parent);
	progress.setWindowTitle(tr("Computing internal structure", "Title of a progress dialog"));
	progress.setLabelText(tr("Processing internally the activities ... please wait"));
	progress.setRange(0, range);
	progress.setModal(true);
	int ttt=0;
		
	Activity* act;
	counter=0;
	
	this->inactiveActivities.clear();
	
	for(int i=0; i<this->activitiesList.size(); i++){
		act=this->activitiesList[i];
		if(act->active){
			progress.setValue(ttt);
			//pqapplication->processEvents();
			if(progress.wasCanceled()){
				RulesImpossible::warning(parent, tr("FET information"), tr("Canceled"));
				return false;
			}
			ttt++;

			counter++;
			act->computeInternalStructure(*this);
		}
		else
			inactiveActivities.insert(act->id);
	}
	
	progress.setValue(range);

	for(int i=0; i<nInternalSubgroups; i++)
		internalSubgroupsList[i]->activitiesForSubgroup.clear();
	for(int i=0; i<nInternalTeachers; i++)
		internalTeachersList[i]->activitiesForTeacher.clear();

	assert(counter<=MAX_ACTIVITIES);
	this->nInternalActivities=counter;
	this->internalActivitiesList.resize(this->nInternalActivities);
	int activei=0;
	for(int ai=0; ai<this->activitiesList.size(); ai++){
		act=this->activitiesList[ai];
		if(act->active){
			this->internalActivitiesList[activei]=*act;
			
			for(int j=0; j<act->iSubgroupsList.count(); j++){
				int k=act->iSubgroupsList.at(j);
				assert(!internalSubgroupsList[k]->activitiesForSubgroup.contains(activei));
				internalSubgroupsList[k]->activitiesForSubgroup.append(activei);
			}
			
			for(int j=0; j<act->iTeachersList.count(); j++){
				int k=act->iTeachersList.at(j);
				assert(!internalTeachersList[k]->activitiesForTeacher.contains(activei));
				internalTeachersList[k]->activitiesForTeacher.append(activei);
			}
			
			activei++;
		}
	}

	/*activitiesForTeacherHash.clear();
	activitiesForSubjectHash.clear();
	activitiesForActivityTagHash.clear();
	activitiesForStudentsSetHash.clear();*/
	activitiesHash.clear();
	for(int i=0; i<nInternalActivities; i++){
		assert(!activitiesHash.contains(internalActivitiesList[i].id));
		activitiesHash.insert(internalActivitiesList[i].id, i);
		
		/*const Activity& act=internalActivitiesList[i];
		
		foreach(QString tch, act.teachersNames){
			QSet<int> set=activitiesForTeacherHash.value(tch, QSet<int>());
			assert(!set.contains(i));
			set.insert(i);
			activitiesForTeacherHash.insert(tch, set);
		}

		QString sbj=act.subjectName;
		QSet<int> set=activitiesForSubjectHash.value(sbj, QSet<int>());
		assert(!set.contains(i));
		set.insert(i);
		activitiesForSubjectHash.insert(sbj, set);

		foreach(QString at, act.activityTagsNames){
			QSet<int> set=activitiesForActivityTagHash.value(at, QSet<int>());
			assert(!set.contains(i));
			set.insert(i);
			activitiesForActivityTagHash.insert(at, set);
		}

		foreach(QString st, act.studentsNames){
			QSet<int> set=activitiesForStudentsSetHash.value(st, QSet<int>());
			assert(!set.contains(i));
			set.insert(i);
			activitiesForStudentsSetHash.insert(st, set);
		}*/
	}

	//activities list for each subject - used for subjects timetable - in order for students and teachers
	activitiesForSubject.resize(nInternalSubjects);
	for(int sb=0; sb<nInternalSubjects; sb++)
		activitiesForSubject[sb].clear();

	for(int i=0; i<this->augmentedYearsList.size(); i++){
		StudentsYear* sty=this->augmentedYearsList[i];

		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];

			for(int k=0; k<stg->subgroupsList.size(); k++){
				StudentsSubgroup* sts=stg->subgroupsList[k];
				
				foreach(int ai, internalSubgroupsList[sts->indexInInternalSubgroupsList]->activitiesForSubgroup)
					if(!activitiesForSubject[internalActivitiesList[ai].subjectIndex].contains(ai))
						activitiesForSubject[internalActivitiesList[ai].subjectIndex].append(ai);
			}
		}
	}
	
	for(int i=0; i<nInternalTeachers; i++){
		foreach(int ai, internalTeachersList[i]->activitiesForTeacher)
			if(!activitiesForSubject[internalActivitiesList[ai].subjectIndex].contains(ai))
				activitiesForSubject[internalActivitiesList[ai].subjectIndex].append(ai);
	}
	/////////////////////////////////////////////////////////////////

	bool ok=true;

	//time constraints
	//progress.reset();
	
	bool skipInactiveTimeConstraints=false;
	
	TimeConstraint* tctr;
	
	QSet<int> toSkipTimeSet;
	
	int _c=0;
	
	for(int tctrindex=0; tctrindex<this->timeConstraintsList.size(); tctrindex++){
		tctr=this->timeConstraintsList[tctrindex];

		if(!tctr->active){
			toSkipTimeSet.insert(tctrindex);
		}
		else if(tctr->hasInactiveActivities(*this)){
			//toSkipTime[tctrindex]=true;
			toSkipTimeSet.insert(tctrindex);
		
			if(!skipInactiveTimeConstraints){
				QString s=tr("The following time constraint is ignored, because it refers to inactive activities:");
				s+="\n";
				s+=tctr->getDetailedDescription(*this);
				
				int t=RulesConstraintIgnored::mediumConfirmation(parent, tr("FET information"), s,
				 tr("Skip rest"), tr("See next"), QString(),
 				 1, 0 );

				if(t==0)
					skipInactiveTimeConstraints=true;
			}
		}
		else{
			//toSkipTime[tctrindex]=false;
			_c++;
		}
	}
	
	internalTimeConstraintsList.resize(_c);
	
	progress.setLabelText(tr("Processing internally the time constraints ... please wait"));
	progress.setRange(0, timeConstraintsList.size());
	ttt=0;
		
	//assert(this->timeConstraintsList.size()<=MAX_TIME_CONSTRAINTS);
	int tctri=0;
	
	for(int tctrindex=0; tctrindex<this->timeConstraintsList.size(); tctrindex++){
		progress.setValue(ttt);
		//pqapplication->processEvents();
		if(progress.wasCanceled()){
			RulesImpossible::warning(parent, tr("FET information"), tr("Canceled"));
			return false;
		}
		ttt++;

		tctr=this->timeConstraintsList[tctrindex];
		
		if(toSkipTimeSet.contains(tctrindex))
			continue;
		
		if(!tctr->computeInternalStructure(parent, *this)){
			//assert(0);
			ok=false;
			continue;
		}
		this->internalTimeConstraintsList[tctri++]=tctr;
	}

	progress.setValue(timeConstraintsList.size());

	this->nInternalTimeConstraints=tctri;
	if(VERBOSE){
		cout<<_c<<" time constraints after first pass (after removing inactive ones)"<<endl;
		cout<<"  "<<this->nInternalTimeConstraints<<" time constraints after second pass (after removing wrong ones)"<<endl;
	}
	assert(_c>=this->nInternalTimeConstraints); //because some constraints may have toSkipTime false, but computeInternalStructure also false
	//assert(this->nInternalTimeConstraints<=MAX_TIME_CONSTRAINTS);
	
	//space constraints
	//progress.reset();
	
	bool skipInactiveSpaceConstraints=false;
	
	SpaceConstraint* sctr;
	
	QSet<int> toSkipSpaceSet;
	
	_c=0;

	for(int sctrindex=0; sctrindex<this->spaceConstraintsList.size(); sctrindex++){
		sctr=this->spaceConstraintsList[sctrindex];

		if(!sctr->active){
			toSkipSpaceSet.insert(sctrindex);
		}
		else if(sctr->hasInactiveActivities(*this)){
			//toSkipSpace[sctrindex]=true;
			toSkipSpaceSet.insert(sctrindex);
		
			if(!skipInactiveSpaceConstraints){
				QString s=tr("The following space constraint is ignored, because it refers to inactive activities:");
				s+="\n";
				s+=sctr->getDetailedDescription(*this);
				
				int t=RulesConstraintIgnored::mediumConfirmation(parent, tr("FET information"), s,
				 tr("Skip rest"), tr("See next"), QString(),
 				 1, 0 );

				if(t==0)
					skipInactiveSpaceConstraints=true;
			}
		}
		else{
			_c++;
			//toSkipSpace[sctrindex]=false;
		}
	}
	
	internalSpaceConstraintsList.resize(_c);
	
	progress.setLabelText(tr("Processing internally the space constraints ... please wait"));
	progress.setRange(0, spaceConstraintsList.size());
	ttt=0;
	//assert(this->spaceConstraintsList.size()<=MAX_SPACE_CONSTRAINTS);

	int sctri=0;

	for(int sctrindex=0; sctrindex<this->spaceConstraintsList.size(); sctrindex++){
		progress.setValue(ttt);
		//pqapplication->processEvents();
		if(progress.wasCanceled()){
			RulesImpossible::warning(parent, tr("FET information"), tr("Canceled"));
			return false;
		}
		ttt++;

		sctr=this->spaceConstraintsList[sctrindex];
	
		if(toSkipSpaceSet.contains(sctrindex))
			continue;
		
		if(!sctr->computeInternalStructure(parent, *this)){
			//assert(0);
			ok=false;
			continue;
		}
		this->internalSpaceConstraintsList[sctri++]=sctr;
	}

	progress.setValue(spaceConstraintsList.size());

	this->nInternalSpaceConstraints=sctri;
	if(VERBOSE){
		cout<<_c<<" space constraints after first pass (after removing inactive ones)"<<endl;
		cout<<"  "<<this->nInternalSpaceConstraints<<" space constraints after second pass (after removing wrong ones)"<<endl;
	}
	assert(_c>=this->nInternalSpaceConstraints); //because some constraints may have toSkipSpace false, but computeInternalStructure also false
	//assert(this->nInternalSpaceConstraints<=MAX_SPACE_CONSTRAINTS);
	
	//group activities in initial order
	if(groupActivitiesInInitialOrderList.count()>0){
		QStringList fetBugs;
		QStringList userErrors;
	
		QSet<int> visitedIds;
		for(int j=0; j<groupActivitiesInInitialOrderList.count(); j++){
			GroupActivitiesInInitialOrderItem &item=groupActivitiesInInitialOrderList[j];
			
			if(!item.active)
				continue;
			
			if(item.ids.count()<2){
				fetBugs.append(tr("All 'group activities in the initial order for timetable generation' items should contain at least two activities ids."
				 " This is not true for item number %1. Please report potential bug.").arg(j+1));
			}

			item.indices.clear();
			foreach(int id, item.ids){
				if(visitedIds.contains(id)){
					userErrors.append(tr("All 'group activities in the initial order for timetable generation' items should have different activities ids."
					 " (Each activity id must appear at most once in all the items.) This is not true for item number %1 and activity id %2.").arg(j+1).arg(id));
				}
				else{
					visitedIds.insert(id);
					int index=activitiesHash.value(id, -1);
					if(index>=0)
						item.indices.append(index);
				}
			}

			if(!fetBugs.isEmpty() || !userErrors.isEmpty()){
				RulesImpossible::warning(parent, tr("FET information"), fetBugs.join("\n\n")+userErrors.join("\n\n"));
				return false;
			}
		}
	}

	//done.
	this->internalStructureComputed=ok;
	
	return ok;
}

void Rules::kill() //clears memory for the rules, destroys them
{
	//Teachers
	while(!teachersList.isEmpty())
		delete teachersList.takeFirst();

	//Subjects
	while(!subjectsList.isEmpty())
		delete subjectsList.takeFirst();

	//Activity tags
	while(!activityTagsList.isEmpty())
		delete activityTagsList.takeFirst();

	//Years
	/*while(!yearsList.isEmpty())
		delete yearsList.takeFirst();*/
		
	//students sets
	QList<StudentsYear*> iyears;
	QList<StudentsGroup*> igroups;
	QList<StudentsSubgroup*> isubgroups;
	foreach(StudentsYear* year, yearsList){
		if(!iyears.contains(year))
			iyears.append(year);
		foreach(StudentsGroup* group, year->groupsList){
			if(!igroups.contains(group))
				igroups.append(group);
			foreach(StudentsSubgroup* subgroup, group->subgroupsList){
				if(!isubgroups.contains(subgroup))
					isubgroups.append(subgroup);
			}
		}
	}
	foreach(StudentsYear* year, iyears){
		assert(year!=NULL);
		delete year;
	}
	foreach(StudentsGroup* group, igroups){
		assert(group!=NULL);
		delete group;
	}
	foreach(StudentsSubgroup* subgroup, isubgroups){
		assert(subgroup!=NULL);
		delete subgroup;
	}	
	yearsList.clear();
	//////////////////

	//kill augmented students sets
	QList<StudentsYear*> ayears;
	QList<StudentsGroup*> agroups;
	QList<StudentsSubgroup*> asubgroups;
	foreach(StudentsYear* year, augmentedYearsList){
		if(!ayears.contains(year))
			ayears.append(year);
		foreach(StudentsGroup* group, year->groupsList){
			if(!agroups.contains(group))
				agroups.append(group);
			foreach(StudentsSubgroup* subgroup, group->subgroupsList){
				if(!asubgroups.contains(subgroup))
					asubgroups.append(subgroup);
			}
		}
	}
	foreach(StudentsYear* year, ayears){
		assert(year!=NULL);
		delete year;
	}
	foreach(StudentsGroup* group, agroups){
		assert(group!=NULL);
		delete group;
	}
	foreach(StudentsSubgroup* subgroup, asubgroups){
		assert(subgroup!=NULL);
		delete subgroup;
	}	
	augmentedYearsList.clear();
	//////////////////
	
	//Activities
	while(!activitiesList.isEmpty())
		delete activitiesList.takeFirst();

	//Time constraints
	while(!timeConstraintsList.isEmpty())
		delete timeConstraintsList.takeFirst();

	//Space constraints
	while(!spaceConstraintsList.isEmpty())
		delete spaceConstraintsList.takeFirst();

	//Buildings
	while(!buildingsList.isEmpty())
		delete buildingsList.takeFirst();

	//Rooms
	while(!roomsList.isEmpty())
		delete roomsList.takeFirst();
		
	groupActivitiesInInitialOrderList.clear();

	activitiesPointerHash.clear();
	bctSet.clear();
	btSet.clear();
	bcsSet.clear();
	apstHash.clear();
	aprHash.clear();
	mdbaHash.clear();
	tnatHash.clear();
	ssnatHash.clear();
	
	teachersHash.clear();
	subjectsHash.clear();
	activityTagsHash.clear();
	studentsHash.clear();
	buildingsHash.clear();
	roomsHash.clear();
	activitiesHash.clear();

	//done
	this->internalStructureComputed=false;
	this->initialized=false;
}

Rules::Rules()
{
	this->initialized=false;
	this->modified=false;
}

Rules::~Rules()
{
	if(this->initialized)
		this->kill();
}

void Rules::setInstitutionName(const QString& newInstitutionName)
{
	this->institutionName=newInstitutionName;
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

void Rules::setComments(const QString& newComments)
{
	this->comments=newComments;
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addTeacher(Teacher* teacher)
{
	for(int i=0; i<this->teachersList.size(); i++){
		Teacher* tch=this->teachersList[i];
		if(tch->name==teacher->name)
			return false;
	}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->teachersList.append(teacher);
	return true;
}

bool Rules::addTeacherFast(Teacher* teacher)
{
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->teachersList.append(teacher);
	return true;
}

int Rules::searchTeacher(const QString& teacherName)
{
	for(int i=0; i<this->teachersList.size(); i++)
		if(this->teachersList.at(i)->name==teacherName)
			return i;

	return -1;
}

bool Rules::removeTeacher(const QString& teacherName)
{
	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];
		bool t=act->removeTeacher(teacherName);
		if(t && act->teachersNames.size()==0){
			this->removeActivity(act->id, act->activityGroupId);
			i=0;
			//(You have to be careful, there can be erased more activities here)
		}
		else
			i++;
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
			ConstraintTeacherNotAvailableTimes* crt_constraint=(ConstraintTeacherNotAvailableTimes*)ctr;
			if(teacherName==crt_constraint->teacher)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_GAPS_PER_WEEK){
			ConstraintTeacherMaxGapsPerWeek* crt_constraint=(ConstraintTeacherMaxGapsPerWeek*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_GAPS_PER_DAY){
			ConstraintTeacherMaxGapsPerDay* crt_constraint=(ConstraintTeacherMaxGapsPerDay*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_HOURS_DAILY){
			ConstraintTeacherMaxHoursDaily* crt_constraint=(ConstraintTeacherMaxHoursDaily*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherMaxHoursContinuously* crt_constraint=(ConstraintTeacherMaxHoursContinuously*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeacherActivityTagMaxHoursContinuously*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeacherActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeacherActivityTagMaxHoursDaily*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MIN_HOURS_DAILY){
			ConstraintTeacherMinHoursDaily* crt_constraint=(ConstraintTeacherMinHoursDaily*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_DAYS_PER_WEEK){
			ConstraintTeacherMaxDaysPerWeek* crt_constraint=(ConstraintTeacherMaxDaysPerWeek*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MIN_DAYS_PER_WEEK){
			ConstraintTeacherMinDaysPerWeek* crt_constraint=(ConstraintTeacherMinDaysPerWeek*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintTeacherIntervalMaxDaysPerWeek* crt_constraint=(ConstraintTeacherIntervalMaxDaysPerWeek*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(teacherName==crt_constraint->p_teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(teacherName==crt_constraint->p_teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else
			i++;
	}


	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
		if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOM){
			ConstraintTeacherHomeRoom* crt_constraint=(ConstraintTeacherHomeRoom*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeSpaceConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOMS){
			ConstraintTeacherHomeRooms* crt_constraint=(ConstraintTeacherHomeRooms*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeSpaceConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintTeacherMaxBuildingChangesPerDay* crt_constraint=(ConstraintTeacherMaxBuildingChangesPerDay*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeSpaceConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintTeacherMaxBuildingChangesPerWeek* crt_constraint=(ConstraintTeacherMaxBuildingChangesPerWeek*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeSpaceConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintTeacherMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintTeacherMinGapsBetweenBuildingChanges*)ctr;
			if(teacherName==crt_constraint->teacherName)
				this->removeSpaceConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else
			i++;
	}


	for(int i=0; i<this->teachersList.size(); i++)
		if(this->teachersList.at(i)->name==teacherName){
			Teacher* tch=this->teachersList[i];
			this->teachersList.removeAt(i);
			delete tch;
			break;
		}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::modifyTeacher(const QString& initialTeacherName, const QString& finalTeacherName)
{
	assert(this->searchTeacher(finalTeacherName)==-1);
	assert(this->searchTeacher(initialTeacherName)>=0);

	//TODO: improve this part
	for(int i=0; i<this->activitiesList.size(); i++)
		this->activitiesList.at(i)->renameTeacher(initialTeacherName, finalTeacherName);

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
			ConstraintTeacherNotAvailableTimes* crt_constraint=(ConstraintTeacherNotAvailableTimes*)ctr;
			if(initialTeacherName == crt_constraint->teacher)
				crt_constraint->teacher=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_GAPS_PER_WEEK){
			ConstraintTeacherMaxGapsPerWeek* crt_constraint=(ConstraintTeacherMaxGapsPerWeek*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_GAPS_PER_DAY){
			ConstraintTeacherMaxGapsPerDay* crt_constraint=(ConstraintTeacherMaxGapsPerDay*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_HOURS_DAILY){
			ConstraintTeacherMaxHoursDaily* crt_constraint=(ConstraintTeacherMaxHoursDaily*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherMaxHoursContinuously* crt_constraint=(ConstraintTeacherMaxHoursContinuously*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeacherActivityTagMaxHoursContinuously*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeacherActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeacherActivityTagMaxHoursDaily*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MIN_HOURS_DAILY){
			ConstraintTeacherMinHoursDaily* crt_constraint=(ConstraintTeacherMinHoursDaily*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_DAYS_PER_WEEK){
			ConstraintTeacherMaxDaysPerWeek* crt_constraint=(ConstraintTeacherMaxDaysPerWeek*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MIN_DAYS_PER_WEEK){
			ConstraintTeacherMinDaysPerWeek* crt_constraint=(ConstraintTeacherMinDaysPerWeek*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintTeacherIntervalMaxDaysPerWeek* crt_constraint=(ConstraintTeacherIntervalMaxDaysPerWeek*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(initialTeacherName == crt_constraint->p_teacherName)
				crt_constraint->p_teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(initialTeacherName == crt_constraint->p_teacherName)
				crt_constraint->p_teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];	

		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	

	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOM){
			ConstraintTeacherHomeRoom* crt_constraint=(ConstraintTeacherHomeRoom*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOMS){
			ConstraintTeacherHomeRooms* crt_constraint=(ConstraintTeacherHomeRooms*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintTeacherMaxBuildingChangesPerDay* crt_constraint=(ConstraintTeacherMaxBuildingChangesPerDay*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintTeacherMaxBuildingChangesPerWeek* crt_constraint=(ConstraintTeacherMaxBuildingChangesPerWeek*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];	

		if(ctr->type==CONSTRAINT_TEACHER_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintTeacherMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintTeacherMinGapsBetweenBuildingChanges*)ctr;
			if(initialTeacherName == crt_constraint->teacherName)
				crt_constraint->teacherName=finalTeacherName;
		}
	}
	


	int t=0;
	for(int i=0; i<this->teachersList.size(); i++){
		Teacher* tch=this->teachersList[i];

		if(tch->name==initialTeacherName){
			tch->name=finalTeacherName;
			t++;
		}
	}
	assert(t<=1);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	if(t==0)
		return false;
	else
		return true;
}

void Rules::sortTeachersAlphabetically()
{
	//qSort(this->teachersList.begin(), this->teachersList.end(), teachersAscending);
	std::stable_sort(this->teachersList.begin(), this->teachersList.end(), teachersAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addSubject(Subject* subject)
{
	for(int i=0; i<this->subjectsList.size(); i++){
		Subject* sbj=this->subjectsList[i];	
		if(sbj->name==subject->name)
			return false;
	}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->subjectsList << subject;
	return true;
}

bool Rules::addSubjectFast(Subject* subject)
{
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->subjectsList << subject;
	return true;
}

int Rules::searchSubject(const QString& subjectName)
{
	for(int i=0; i<this->subjectsList.size(); i++)
		if(this->subjectsList.at(i)->name == subjectName)
			return i;

	return -1;
}

bool Rules::removeSubject(const QString& subjectName)
{
	//check the activities first
	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];
		if(act->subjectName==subjectName){
			this->removeActivity(act->id, act->activityGroupId);
			i=0;
			//(You have to be careful, there can be erased more activities here)
		}
		else
			i++;
	}
	
	//delete the time constraints related to this subject

	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(subjectName==crt_constraint->p_subjectName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(subjectName==crt_constraint->subjectName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(subjectName==crt_constraint->subjectName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(subjectName==crt_constraint->p_subjectName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(subjectName==crt_constraint->subjectName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else
			i++;
	}

	//delete the space constraints related to this subject
	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOM){
			ConstraintSubjectPreferredRoom* c=(ConstraintSubjectPreferredRoom*)ctr;

			if(c->subjectName == subjectName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOMS){
			ConstraintSubjectPreferredRooms* c=(ConstraintSubjectPreferredRooms*)ctr;

			if(c->subjectName == subjectName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;

			if(c->subjectName == subjectName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;

			if(c->subjectName == subjectName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}

	//remove the subject from the list
	for(int i=0; i<this->subjectsList.size(); i++)
		if(this->subjectsList[i]->name==subjectName){
			Subject* sbj=this->subjectsList[i];
			this->subjectsList.removeAt(i);
			delete sbj;
			break;
		}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::modifySubject(const QString& initialSubjectName, const QString& finalSubjectName)
{
	assert(this->searchSubject(finalSubjectName)==-1);
	assert(this->searchSubject(initialSubjectName)>=0);

	//check the activities first
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];

		if( act->subjectName == initialSubjectName)
			act->subjectName=finalSubjectName;
	}
	
	//modify the time constraints related to this subject
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(initialSubjectName == crt_constraint->p_subjectName)
				crt_constraint->p_subjectName=finalSubjectName;
		}
	}

	//modify the time constraints related to this subject
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(initialSubjectName == crt_constraint->subjectName)
				crt_constraint->subjectName=finalSubjectName;
		}
	}

	//modify the time constraints related to this subject
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(initialSubjectName == crt_constraint->subjectName)
				crt_constraint->subjectName=finalSubjectName;
		}
	}

	//modify the time constraints related to this subject
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(initialSubjectName == crt_constraint->p_subjectName)
				crt_constraint->p_subjectName=finalSubjectName;
		}
	}

	//modify the time constraints related to this subject
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(initialSubjectName == crt_constraint->subjectName)
				crt_constraint->subjectName=finalSubjectName;
		}
	}

	//modify the space constraints related to this subject
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOM){
			ConstraintSubjectPreferredRoom* c=(ConstraintSubjectPreferredRoom*)ctr;
			if(c->subjectName == initialSubjectName)
				c->subjectName=finalSubjectName;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOMS){
			ConstraintSubjectPreferredRooms* c=(ConstraintSubjectPreferredRooms*)ctr;
			if(c->subjectName == initialSubjectName)
				c->subjectName=finalSubjectName;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;
			if(c->subjectName == initialSubjectName)
				c->subjectName=finalSubjectName;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;
			if(c->subjectName == initialSubjectName)
				c->subjectName=finalSubjectName;
		}
	}


	//rename the subject in the list
	int t=0;
	for(int i=0; i<this->subjectsList.size(); i++){
		Subject* sbj=this->subjectsList[i];

		if(sbj->name==initialSubjectName){
			t++;
			sbj->name=finalSubjectName;
		}
	}
	assert(t<=1);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

void Rules::sortSubjectsAlphabetically()
{
	//qSort(this->subjectsList.begin(), this->subjectsList.end(), subjectsAscending);
	std::stable_sort(this->subjectsList.begin(), this->subjectsList.end(), subjectsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addActivityTag(ActivityTag* activityTag)
{
	for(int i=0; i<this->activityTagsList.size(); i++){
		ActivityTag* sbt=this->activityTagsList[i];

		if(sbt->name==activityTag->name)
			return false;
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->activityTagsList << activityTag;
	return true;
}

bool Rules::addActivityTagFast(ActivityTag* activityTag)
{
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	this->activityTagsList << activityTag;
	return true;
}

int Rules::searchActivityTag(const QString& activityTagName)
{
	for(int i=0; i<this->activityTagsList.size(); i++)
		if(this->activityTagsList.at(i)->name==activityTagName)
			return i;

	return -1;
}

bool Rules::removeActivityTag(const QString& activityTagName)
{
	//check the activities first
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];

		//if( act->activityTagName == activityTagName)
		//	act->activityTagName="";
		if( act->activityTagsNames.contains(activityTagName) )
			act->activityTagsNames.removeAll(activityTagName);
	}
	
	//delete the time constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		
		if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeacherActivityTagMaxHoursContinuously*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeacherActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeacherActivityTagMaxHoursDaily*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHERS_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeachersActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeachersActivityTagMaxHoursContinuously*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_TEACHERS_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeachersActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeachersActivityTagMaxHoursDaily*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsActivityTagMaxHoursContinuously*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsActivityTagMaxHoursDaily*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(activityTagName == crt_constraint->p_activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(activityTagName == crt_constraint->p_activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(activityTagName == crt_constraint->activityTagName)
				this->removeTimeConstraint(ctr); //single constraint removal
			else
				i++;
		}
		else
			i++;
	}

	//delete the space constraints related to this activity tag
	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
		
		if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;

			if(c->activityTagName == activityTagName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;

			if(c->activityTagName == activityTagName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintActivityTagPreferredRoom* c=(ConstraintActivityTagPreferredRoom*)ctr;

			if(c->activityTagName == activityTagName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintActivityTagPreferredRooms* c=(ConstraintActivityTagPreferredRooms*)ctr;

			if(c->activityTagName == activityTagName)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}

	//remove the activity tag from the list
	for(int i=0; i<this->activityTagsList.size(); i++)
		if(this->activityTagsList[i]->name==activityTagName){
			ActivityTag* sbt=this->activityTagsList[i];
			this->activityTagsList.removeAt(i);
			delete sbt;
			break;
		}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::modifyActivityTag(const QString& initialActivityTagName, const QString& finalActivityTagName)
{
	assert(this->searchActivityTag(finalActivityTagName)==-1);
	assert(this->searchActivityTag(initialActivityTagName)>=0);

	//check the activities first
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];

		//if( act->activityTagName == initialActivityTagName)
		//	act->activityTagName=finalActivityTagName;
		for(int kk=0; kk<act->activityTagsNames.count(); kk++)
			if(act->activityTagsNames.at(kk)==initialActivityTagName)
				act->activityTagsNames[kk]=finalActivityTagName;
	}
	
	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeacherActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeacherActivityTagMaxHoursContinuously*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_TEACHER_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeacherActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeacherActivityTagMaxHoursDaily*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_TEACHERS_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintTeachersActivityTagMaxHoursContinuously* crt_constraint=(ConstraintTeachersActivityTagMaxHoursContinuously*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_TEACHERS_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintTeachersActivityTagMaxHoursDaily* crt_constraint=(ConstraintTeachersActivityTagMaxHoursDaily*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_STUDENTS_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsActivityTagMaxHoursContinuously*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_STUDENTS_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsActivityTagMaxHoursDaily*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}

	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(initialActivityTagName == crt_constraint->p_activityTagName)
				crt_constraint->p_activityTagName=finalActivityTagName;
		}
	}

	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}

	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}

	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(initialActivityTagName == crt_constraint->p_activityTagName)
				crt_constraint->p_activityTagName=finalActivityTagName;
		}
	}

	//modify the constraints related to this activity tag
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(initialActivityTagName == crt_constraint->activityTagName)
				crt_constraint->activityTagName=finalActivityTagName;
		}
	}

	//modify the space constraints related to this subject tag
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;
			if(c->activityTagName == initialActivityTagName)
				c->activityTagName=finalActivityTagName;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;
			if(c->activityTagName == initialActivityTagName)
				c->activityTagName=finalActivityTagName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintActivityTagPreferredRoom* c=(ConstraintActivityTagPreferredRoom*)ctr;
			if(c->activityTagName == initialActivityTagName)
				c->activityTagName=finalActivityTagName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintActivityTagPreferredRooms* c=(ConstraintActivityTagPreferredRooms*)ctr;
			if(c->activityTagName == initialActivityTagName)
				c->activityTagName=finalActivityTagName;
		}
	}

	//rename the activity tag in the list
	int t=0;
	
	for(int i=0; i<this->activityTagsList.size(); i++){
		ActivityTag* sbt=this->activityTagsList[i];

		if(sbt->name==initialActivityTagName){
			t++;
			sbt->name=finalActivityTagName;
		}
	}
	
	assert(t<=1);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

void Rules::sortActivityTagsAlphabetically()
{
	//qSort(this->activityTagsList.begin(), this->activityTagsList.end(), activityTagsAscending);
	std::stable_sort(this->activityTagsList.begin(), this->activityTagsList.end(), activityTagsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::setsShareStudents(const QString& studentsSet1, const QString& studentsSet2)
{
	StudentsSet* s1=this->searchStudentsSet(studentsSet1);
	StudentsSet* s2=this->searchStudentsSet(studentsSet2);
	assert(s1!=NULL);
	assert(s2!=NULL);
	
	QSet<QString> downwardSets1;
	
	if(s1->type==STUDENTS_YEAR){
		StudentsYear* year1=(StudentsYear*)s1;
		downwardSets1.insert(year1->name);
		foreach(StudentsGroup* group1, year1->groupsList){
			downwardSets1.insert(group1->name);
			foreach(StudentsSubgroup* subgroup1, group1->subgroupsList)
				downwardSets1.insert(subgroup1->name);
		}
	}
	else if(s1->type==STUDENTS_GROUP){
		StudentsGroup* group1=(StudentsGroup*)s1;
		downwardSets1.insert(group1->name);
		foreach(StudentsSubgroup* subgroup1, group1->subgroupsList)
			downwardSets1.insert(subgroup1->name);
	}
	else if(s1->type==STUDENTS_SUBGROUP){
		StudentsSubgroup* subgroup1=(StudentsSubgroup*)s1;
		downwardSets1.insert(subgroup1->name);
	}
	else
		assert(0);
		
	if(s2->type==STUDENTS_YEAR){
		StudentsYear* year2=(StudentsYear*)s2;
		if(downwardSets1.contains(year2->name))
			return true;
		foreach(StudentsGroup* group2, year2->groupsList){
			if(downwardSets1.contains(group2->name))
				return true;
			foreach(StudentsSubgroup* subgroup2, group2->subgroupsList)
				if(downwardSets1.contains(subgroup2->name))
					return true;
		}
	}
	else if(s2->type==STUDENTS_GROUP){
		StudentsGroup* group2=(StudentsGroup*)s2;
		if(downwardSets1.contains(group2->name))
			return true;
		foreach(StudentsSubgroup* subgroup2, group2->subgroupsList)
			if(downwardSets1.contains(subgroup2->name))
				return true;
	}
	else if(s2->type==STUDENTS_SUBGROUP){
		StudentsSubgroup* subgroup2=(StudentsSubgroup*)s2;
		if(downwardSets1.contains(subgroup2->name))
			return true;
	}
	else
		assert(0);
	
	return false;
	
}

bool Rules::augmentedSetsShareStudentsFaster(const QString& studentsSet1, const QString& studentsSet2)
{
	//StudentsSet* s1=this->searchStudentsSet(studentsSet1);
	StudentsSet* s1=studentsHash.value(studentsSet1, NULL);
	//StudentsSet* s2=this->searchStudentsSet(studentsSet2);
	StudentsSet* s2=studentsHash.value(studentsSet2, NULL);
	assert(s1!=NULL);
	assert(s2!=NULL);
	
	QSet<QString> downwardSets1;
	
	if(s1->type==STUDENTS_YEAR){
		StudentsYear* year1=(StudentsYear*)s1;
		downwardSets1.insert(year1->name);
		foreach(StudentsGroup* group1, year1->groupsList){
			downwardSets1.insert(group1->name);
			foreach(StudentsSubgroup* subgroup1, group1->subgroupsList)
				downwardSets1.insert(subgroup1->name);
		}
	}
	else if(s1->type==STUDENTS_GROUP){
		StudentsGroup* group1=(StudentsGroup*)s1;
		downwardSets1.insert(group1->name);
		foreach(StudentsSubgroup* subgroup1, group1->subgroupsList)
			downwardSets1.insert(subgroup1->name);
	}
	else if(s1->type==STUDENTS_SUBGROUP){
		StudentsSubgroup* subgroup1=(StudentsSubgroup*)s1;
		downwardSets1.insert(subgroup1->name);
	}
	else
		assert(0);
		
	if(s2->type==STUDENTS_YEAR){
		StudentsYear* year2=(StudentsYear*)s2;
		if(downwardSets1.contains(year2->name))
			return true;
		foreach(StudentsGroup* group2, year2->groupsList){
			if(downwardSets1.contains(group2->name))
				return true;
			foreach(StudentsSubgroup* subgroup2, group2->subgroupsList)
				if(downwardSets1.contains(subgroup2->name))
					return true;
		}
	}
	else if(s2->type==STUDENTS_GROUP){
		StudentsGroup* group2=(StudentsGroup*)s2;
		if(downwardSets1.contains(group2->name))
			return true;
		foreach(StudentsSubgroup* subgroup2, group2->subgroupsList)
			if(downwardSets1.contains(subgroup2->name))
				return true;
	}
	else if(s2->type==STUDENTS_SUBGROUP){
		StudentsSubgroup* subgroup2=(StudentsSubgroup*)s2;
		if(downwardSets1.contains(subgroup2->name))
			return true;
	}
	else
		assert(0);
	
	return false;
	
}

StudentsSet* Rules::searchStudentsSet(const QString& setName)
{
	for(int i=0; i<this->yearsList.size(); i++){
		StudentsYear* sty=this->yearsList[i];
		if(sty->name==setName)
			return sty;
		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];
			if(stg->name==setName)
				return stg;
			for(int k=0; k<stg->subgroupsList.size(); k++){
				StudentsSubgroup* sts=stg->subgroupsList[k];
				if(sts->name==setName)
					return sts;
			}
		}
	}
	return NULL;
}

StudentsSet* Rules::searchAugmentedStudentsSet(const QString& setName)
{
	for(int i=0; i<this->augmentedYearsList.size(); i++){
		StudentsYear* sty=this->augmentedYearsList[i];
		if(sty->name==setName)
			return sty;
		for(int j=0; j<sty->groupsList.size(); j++){
			StudentsGroup* stg=sty->groupsList[j];
			if(stg->name==setName)
				return stg;
			for(int k=0; k<stg->subgroupsList.size(); k++){
				StudentsSubgroup* sts=stg->subgroupsList[k];
				if(sts->name==setName)
					return sts;
			}
		}
	}
	return NULL;
}

bool Rules::addYear(StudentsYear* year)
{
	//already existing?
	if(this->searchStudentsSet(year->name)!=NULL)
		return false;
	this->yearsList << year;
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::addYearFast(StudentsYear* year)
{
	this->yearsList << year;
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::removeYear(const QString& yearName)
{
	StudentsYear* year=NULL;
	foreach(StudentsYear* ty, this->yearsList){
		if(ty->name==yearName){
			year=ty;
			break;
		}
	}

	//StudentsYear* year=(StudentsYear*)searchStudentsSet(yearName);
	assert(year!=NULL);
	int nStudents=year->numberOfStudents;
	while(year->groupsList.size()>0){
		QString groupName=year->groupsList[0]->name;
		this->removeGroup(yearName, groupName);
	}

	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];
		bool t=act->removeStudents(*this, yearName, nStudents);
		
		if(t && act->studentsNames.count()==0){
			this->removeActivity(act->id, act->activityGroupId);
			i=0;
			//(You have to be careful, there can be erased more activities here)
		}
		else
			i++;
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
	
		bool erased=false;

		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(yearName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(yearName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(yearName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}

	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
	
		bool erased=false;

		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(yearName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}

	for(int i=0; i<this->yearsList.size(); i++)
		if(this->yearsList.at(i)->name==yearName){
			delete this->yearsList[i]; //added in version 4.0.2
			this->yearsList.removeAt(i);
			break;
		}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

int Rules::searchYear(const QString& yearName)
{
	for(int i=0; i<this->yearsList.size(); i++)
		if(this->yearsList[i]->name==yearName)
			return i;

	return -1;
}

int Rules::searchAugmentedYear(const QString& yearName)
{
	for(int i=0; i<this->augmentedYearsList.size(); i++)
		if(this->augmentedYearsList[i]->name==yearName)
			return i;

	return -1;
}

bool Rules::modifyYear(const QString& initialYearName, const QString& finalYearName, int finalNumberOfStudents)
{
	StudentsSet* _initialSet=searchStudentsSet(initialYearName);
	assert(_initialSet!=NULL);
	int _initialNumberOfStudents=_initialSet->numberOfStudents;

	QString _initialYearName=initialYearName;

	assert(searchYear(_initialYearName)>=0);
	StudentsSet* _ss=searchStudentsSet(finalYearName);
	assert(_ss==NULL || _initialYearName==finalYearName);

	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		act->renameStudents(*this, _initialYearName, finalYearName, _initialNumberOfStudents, finalNumberOfStudents);
	}

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(_initialYearName == crt_constraint->students)
				crt_constraint->students=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(_initialYearName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(_initialYearName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
	}

	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(_initialYearName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalYearName;
		}
	}

	int t=0;
	
	for(int i=0; i<this->yearsList.size(); i++){
		StudentsYear* sty=this->yearsList[i];
	
		if(sty->name==_initialYearName){
			sty->name=finalYearName;
			sty->numberOfStudents=finalNumberOfStudents;
			t++;
			
			/*for(int j=0; j<sty->groupsList.size(); j++){
				StudentsGroup* stg=sty->groupsList[j];

				if(stg->name.right(11)==" WHOLE YEAR" && stg->name.left(stg->name.length()-11)==_initialYearName)
					this->modifyGroup(sty->name, stg->name, sty->name+" WHOLE YEAR", stg->numberOfStudents);
			}*/
		}
	}
		
	assert(t<=1);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	if(t==0)
		return false;
	else
		return true;
}

void Rules::sortYearsAlphabetically()
{
	//qSort(this->yearsList.begin(), this->yearsList.end(), yearsAscending);
	std::stable_sort(this->yearsList.begin(), this->yearsList.end(), yearsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addGroup(const QString& yearName, StudentsGroup* group)
{
	StudentsYear* sty=NULL;
	for(int i=0; i<this->yearsList.size(); i++){
		sty=yearsList[i];
		if(sty->name==yearName)
			break;
	}
	assert(sty);
	
	for(int i=0; i<sty->groupsList.size(); i++){
		StudentsGroup* stg=sty->groupsList[i];
		if(stg->name==group->name)
			return false;
	}
	
	sty->groupsList << group; //append

	/*
	foreach(StudentsYear* y, yearsList)
		foreach(StudentsGroup* g, y->groupsList)
			if(g->name==group->name)
				g->numberOfStudents=group->numberOfStudents;*/

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::addGroupFast(StudentsYear* year, StudentsGroup* group)
{
	year->groupsList << group; //append

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::removeGroup(const QString& yearName, const QString& groupName)
{
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	StudentsYear* year=NULL;
	foreach(StudentsYear* ty, this->yearsList){
		if(ty->name==yearName){
			year=ty;
			break;
		}
	}
	assert(year!=NULL);
	
	StudentsGroup* group=NULL;
	foreach(StudentsGroup* tg, year->groupsList){
		if(tg->name==groupName){
			group=tg;
			break;
		}
	}

	//StudentsGroup* group=(StudentsGroup*)searchStudentsSet(groupName);
	assert(group!=NULL);
	int nStudents=group->numberOfStudents;
	
	bool stillExistsWithSamePointer=false;
	foreach(StudentsYear* ty, this->yearsList){
		if(ty->name!=yearName){
			foreach(StudentsGroup* tg, ty->groupsList){
				if(tg==group){
					stillExistsWithSamePointer=true;
					break;
				}
			}
		}
		if(stillExistsWithSamePointer)
			break;
	}
	
	if(!stillExistsWithSamePointer){
		while(group->subgroupsList.size()>0){
			QString subgroupName=group->subgroupsList[0]->name;
			this->removeSubgroup(yearName, groupName, subgroupName);
		}
	}

	StudentsYear* sty=NULL;
	for(int i=0; i<this->yearsList.size(); i++){
		sty=yearsList[i];
		if(sty->name==yearName)
			break;
	}
	assert(sty);
	assert(sty==year);

	StudentsGroup* stg=NULL;
	for(int i=0; i<sty->groupsList.size(); i++){
		stg=sty->groupsList[i];
		if(stg->name==groupName){
			sty->groupsList.removeAt(i);
			break;
		}
	}
	
	assert(stg);

	if(this->searchStudentsSet(stg->name)!=NULL){
		//group still exists
		
		//with the same pointer??? (leak bug fix on 6 Jan 2010 in FET-5.12.1)
		bool foundSamePointer=false;
		foreach(StudentsYear* year, yearsList){
			foreach(StudentsGroup* group, year->groupsList){
				if(group==stg){
					foundSamePointer=true;
					break;
				}
			}
			if(foundSamePointer)
				break;
		}
		
		if(!foundSamePointer)
			delete stg;
		/////////////////////////////////////////////////////////////////////
		
		return true;
	}

	delete stg;

	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];

		bool t=act->removeStudents(*this, groupName, nStudents);
		if(t && act->studentsNames.count()==0){
			this->removeActivity(act->id, act->activityGroupId);
			i=0;
			//(You have to be careful, there can be erased more activities here)
		}
		else
			i++;
	}

	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		bool erased=false;
		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(groupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(groupName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(groupName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}

	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		bool erased=false;
		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(groupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}

	return true;
}

int Rules::searchGroup(const QString& yearName, const QString& groupName)
{
	StudentsYear* sty=this->yearsList[this->searchYear(yearName)];
	assert(sty);
	
	for(int i=0; i<sty->groupsList.size(); i++)
		if(sty->groupsList[i]->name==groupName)
			return i;
	
	return -1;
}

int Rules::searchAugmentedGroup(const QString& yearName, const QString& groupName)
{
	StudentsYear* sty=this->augmentedYearsList[this->searchAugmentedYear(yearName)];
	assert(sty);
	
	for(int i=0; i<sty->groupsList.size(); i++)
		if(sty->groupsList[i]->name==groupName)
			return i;
	
	return -1;
}

bool Rules::modifyGroup(const QString& yearName, const QString& initialGroupName, const QString& finalGroupName, int finalNumberOfStudents)
{
	StudentsSet* _initialSet=searchStudentsSet(initialGroupName);
	assert(_initialSet!=NULL);
	int _initialNumberOfStudents=_initialSet->numberOfStudents;

	//cout<<"Begin: initialGroupName=='"<<qPrintableinitialGroupName<<"'"<<endl;
	
	QString _initialGroupName=initialGroupName;

	assert(searchGroup(yearName, _initialGroupName)>=0);
	StudentsSet* _ss=searchStudentsSet(finalGroupName);
	assert(_ss==NULL || _initialGroupName==finalGroupName);

	StudentsYear* sty=NULL;
	for(int i=0; i<this->yearsList.size(); i++){
		sty=this->yearsList[i];
		if(sty->name==yearName)
			break;
	}
	assert(sty);
	
	StudentsGroup* stg=NULL;
	for(int i=0; i<sty->groupsList.size(); i++){
		stg=sty->groupsList[i];
		if(stg->name==_initialGroupName){
			stg->name=finalGroupName;
			stg->numberOfStudents=finalNumberOfStudents;

			break;
		}
	}
	assert(stg);
	
	if(_ss!=NULL){ //In case it only changes the number of students, make the same number of students in all groups with this name
		assert(_initialGroupName==finalGroupName);
		foreach(StudentsYear* year, yearsList)
			foreach(StudentsGroup* group, year->groupsList)
				if(group->name==finalGroupName)
					group->numberOfStudents=finalNumberOfStudents;
	}

	for(int i=0; i<this->activitiesList.size(); i++)
		this->activitiesList[i]->renameStudents(*this, _initialGroupName, finalGroupName, _initialNumberOfStudents, finalNumberOfStudents);
	
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(_initialGroupName == crt_constraint->students)
				crt_constraint->students=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(_initialGroupName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(_initialGroupName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
	}

	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(_initialGroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalGroupName;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

void Rules::sortGroupsAlphabetically(const QString& yearName)
{
	StudentsYear* sty=this->yearsList[this->searchYear(yearName)];
	assert(sty);

	//qSort(sty->groupsList.begin(), sty->groupsList.end(), groupsAscending);
	std::stable_sort(sty->groupsList.begin(), sty->groupsList.end(), groupsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addSubgroup(const QString& yearName, const QString& groupName, StudentsSubgroup* subgroup)
{
	StudentsYear* sty=this->yearsList.at(this->searchYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchGroup(yearName, groupName));
	assert(stg);


	for(int i=0; i<stg->subgroupsList.size(); i++){
		StudentsSubgroup* sts=stg->subgroupsList[i];
		if(sts->name==subgroup->name)
			return false;
	}
	
	stg->subgroupsList << subgroup; //append

	/*
	foreach(StudentsYear* y, yearsList)
		foreach(StudentsGroup* g, y->groupsList)
			foreach(StudentsSubgroup* s, g->subgroupsList)
				if(s->name==subgroup->name)
					s->numberOfStudents=subgroup->numberOfStudents;*/

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::addSubgroupFast(StudentsYear* year, StudentsGroup* group, StudentsSubgroup* subgroup)
{
	Q_UNUSED(year);

	group->subgroupsList << subgroup; //append

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

bool Rules::removeSubgroup(const QString& yearName, const QString& groupName, const QString& subgroupName)
{
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	StudentsSubgroup* subgroup=(StudentsSubgroup*)searchStudentsSet(subgroupName);
	assert(subgroup!=NULL);
	int nStudents=subgroup->numberOfStudents;

	StudentsYear* sty=this->yearsList.at(this->searchYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchGroup(yearName, groupName));
	assert(stg);

	StudentsSubgroup* sts=NULL;
	for(int i=0; i<stg->subgroupsList.size(); i++){
		sts=stg->subgroupsList[i];
		if(sts->name==subgroupName){
			stg->subgroupsList.removeAt(i);
			break;
		}
	}
	
	assert(sts!=NULL);

	if(this->searchStudentsSet(sts->name)!=NULL){
		//subgroup still exists, in other group
		
		//with the same pointer??? (leak bug fix on 6 Jan 2010 in FET-5.12.1)
		bool foundSamePointer=false;
		foreach(StudentsYear* year, yearsList){
			foreach(StudentsGroup* group, year->groupsList){
				foreach(StudentsSubgroup* subgroup, group->subgroupsList){
					if(subgroup==sts){
						foundSamePointer=true;
						break;
					}
				}
				if(foundSamePointer)
					break;
			}
			if(foundSamePointer)
				break;
		}
		
		if(!foundSamePointer)
			delete sts;
		/////////////////////////////////////////////////////////////////////
		
		return true;
	}

	delete sts;

	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];

		bool t=act->removeStudents(*this, subgroupName, nStudents);
		if(t && act->studentsNames.count()==0){
			this->removeActivity(act->id, act->activityGroupId);
			i=0;
			//(You have to be careful, there can be erased more activities here)
		}
		else
			i++;
	}
	
	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		bool erased=false;
		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(subgroupName == crt_constraint->students){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(subgroupName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(subgroupName == crt_constraint->p_studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeTimeConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}
	
	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		bool erased=false;
		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(subgroupName == crt_constraint->studentsName){
				this->removeSpaceConstraint(ctr);
				erased=true;
			}
		}

		if(!erased)
			i++;
	}

	return true;
}

int Rules::searchSubgroup(const QString& yearName, const QString& groupName, const QString& subgroupName)
{
	StudentsYear* sty=this->yearsList.at(this->searchYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchGroup(yearName, groupName));
	assert(stg);
	
	for(int i=0; i<stg->subgroupsList.size(); i++)
		if(stg->subgroupsList[i]->name==subgroupName)
			return i;
	
	return -1;
}

int Rules::searchAugmentedSubgroup(const QString& yearName, const QString& groupName, const QString& subgroupName)
{
	StudentsYear* sty=this->augmentedYearsList.at(this->searchAugmentedYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchAugmentedGroup(yearName, groupName));
	assert(stg);
	
	for(int i=0; i<stg->subgroupsList.size(); i++)
		if(stg->subgroupsList[i]->name==subgroupName)
			return i;
	
	return -1;
}

bool Rules::modifySubgroup(const QString& yearName, const QString& groupName, const QString& initialSubgroupName, const QString& finalSubgroupName, int finalNumberOfStudents)
{
	StudentsSet* _initialSet=searchStudentsSet(initialSubgroupName);
	assert(_initialSet!=NULL);
	int _initialNumberOfStudents=_initialSet->numberOfStudents;

	QString _initialSubgroupName=initialSubgroupName;

	assert(searchSubgroup(yearName, groupName, _initialSubgroupName)>=0);
	StudentsSet* _ss=searchStudentsSet(finalSubgroupName);
	assert(_ss==NULL || _initialSubgroupName==finalSubgroupName);

	StudentsYear* sty=this->yearsList.at(this->searchYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchGroup(yearName, groupName));
	assert(stg);

	StudentsSubgroup* sts=NULL;
	for(int i=0; i<stg->subgroupsList.size(); i++){
		sts=stg->subgroupsList[i];

		if(sts->name==_initialSubgroupName){
			sts->name=finalSubgroupName;
			sts->numberOfStudents=finalNumberOfStudents;
			break;
		}
	}
	assert(sts);

	if(_ss!=NULL){ //In case it only changes the number of students, make the same number of students in all subgroups with this name
		assert(_initialSubgroupName==finalSubgroupName);
		foreach(StudentsYear* year, yearsList)
			foreach(StudentsGroup* group, year->groupsList)
				foreach(StudentsSubgroup* subgroup, group->subgroupsList)
					if(subgroup->name==finalSubgroupName)
						subgroup->numberOfStudents=finalNumberOfStudents;
	}

	//TODO: improve this part
	for(int i=0; i<this->activitiesList.size(); i++)
		this->activitiesList[i]->renameStudents(*this, _initialSubgroupName, finalSubgroupName, _initialNumberOfStudents, finalNumberOfStudents);

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* crt_constraint=(ConstraintStudentsSetNotAvailableTimes*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_DAILY){
			ConstraintStudentsSetMaxHoursDaily* crt_constraint=(ConstraintStudentsSetMaxHoursDaily*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetMaxDaysPerWeek*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_INTERVAL_MAX_DAYS_PER_WEEK){
			ConstraintStudentsSetIntervalMaxDaysPerWeek* crt_constraint=(ConstraintStudentsSetIntervalMaxDaysPerWeek*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetMaxHoursContinuously*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_CONTINUOUSLY){
			ConstraintStudentsSetActivityTagMaxHoursContinuously* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursContinuously*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_ACTIVITY_TAG_MAX_HOURS_DAILY){
			ConstraintStudentsSetActivityTagMaxHoursDaily* crt_constraint=(ConstraintStudentsSetActivityTagMaxHoursDaily*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_HOURS_DAILY){
			ConstraintStudentsSetMinHoursDaily* crt_constraint=(ConstraintStudentsSetMinHoursDaily*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_EARLY_MAX_BEGINNINGS_AT_SECOND_HOUR){
			ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* crt_constraint=(ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_WEEK){
			ConstraintStudentsSetMaxGapsPerWeek* crt_constraint=(ConstraintStudentsSetMaxGapsPerWeek*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_GAPS_PER_DAY){
			ConstraintStudentsSetMaxGapsPerDay* crt_constraint=(ConstraintStudentsSetMaxGapsPerDay*)ctr;
			if(_initialSubgroupName == crt_constraint->students)
				crt_constraint->students=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintActivitiesPreferredTimeSlots* crt_constraint=(ConstraintActivitiesPreferredTimeSlots*)ctr;
			if(_initialSubgroupName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintActivitiesPreferredStartingTimes* crt_constraint=(ConstraintActivitiesPreferredStartingTimes*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_END_STUDENTS_DAY){
			ConstraintActivitiesEndStudentsDay* crt_constraint=(ConstraintActivitiesEndStudentsDay*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS){
			ConstraintSubactivitiesPreferredTimeSlots* crt_constraint=(ConstraintSubactivitiesPreferredTimeSlots*)ctr;
			if(_initialSubgroupName == crt_constraint->p_studentsName)
				crt_constraint->p_studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES){
			ConstraintSubactivitiesPreferredStartingTimes* crt_constraint=(ConstraintSubactivitiesPreferredStartingTimes*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
	}

	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];

		if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* crt_constraint=(ConstraintStudentsSetHomeRoom*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* crt_constraint=(ConstraintStudentsSetHomeRooms*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_DAY){
			ConstraintStudentsSetMaxBuildingChangesPerDay* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerDay*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MAX_BUILDING_CHANGES_PER_WEEK){
			ConstraintStudentsSetMaxBuildingChangesPerWeek* crt_constraint=(ConstraintStudentsSetMaxBuildingChangesPerWeek*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_MIN_GAPS_BETWEEN_BUILDING_CHANGES){
			ConstraintStudentsSetMinGapsBetweenBuildingChanges* crt_constraint=(ConstraintStudentsSetMinGapsBetweenBuildingChanges*)ctr;
			if(_initialSubgroupName == crt_constraint->studentsName)
				crt_constraint->studentsName=finalSubgroupName;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

void Rules::sortSubgroupsAlphabetically(const QString& yearName, const QString& groupName)
{
	StudentsYear* sty=this->yearsList.at(this->searchYear(yearName));
	assert(sty);
	StudentsGroup* stg=sty->groupsList.at(this->searchGroup(yearName, groupName));
	assert(stg);

	//qSort(stg->subgroupsList.begin(), stg->subgroupsList.end(), subgroupsAscending);
	std::stable_sort(stg->subgroupsList.begin(), stg->subgroupsList.end(), subgroupsAscending);
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addSimpleActivity(
	QWidget* parent,
	int _id,
	int _activityGroupId,
	const QStringList& _teachersNames,
	const QString& _subjectName,
	const QStringList& _activityTagsNames,
	const QStringList& _studentsNames,
	int _duration, /*duration, in hours*/
	int _totalDuration,
	bool _active,
	bool _computeNTotalStudents,
	int _nTotalStudents)
{
	//check for duplicates - idea and code by Volker Dirr
	int t=QStringList(_teachersNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate teachers - please correct that")
		 .arg(_id).arg(t));

	t=QStringList(_studentsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate students sets - please correct that")
		 .arg(_id).arg(t));

	t=QStringList(_activityTagsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate activity tags - please correct that")
		 .arg(_id).arg(t));

	Activity *act=new Activity(*this, _id, _activityGroupId, _teachersNames, _subjectName, _activityTagsNames,
		_studentsNames, _duration, _totalDuration, _active, _computeNTotalStudents, _nTotalStudents);

	this->activitiesList << act; //append
	
	assert(!activitiesPointerHash.contains(act->id));
	activitiesPointerHash.insert(act->id, act);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

bool Rules::addSimpleActivityRulesFast(
	QWidget* parent,
	int _id,
	int _activityGroupId,
	const QStringList& _teachersNames,
	const QString& _subjectName,
	const QStringList& _activityTagsNames,
	const QStringList& _studentsNames,
	int _duration, /*duration, in hours*/
	int _totalDuration,
	bool _active,
	bool _computeNTotalStudents,
	int _nTotalStudents,
	int _computedNumberOfStudents)
{
	//check for duplicates - idea and code by Volker Dirr
	int t=QStringList(_teachersNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate teachers - please correct that")
		 .arg(_id).arg(t));

	t=QStringList(_studentsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate students sets - please correct that")
		 .arg(_id).arg(t));

	t=QStringList(_activityTagsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activity with Id=%1 contains %2 duplicate activity tags - please correct that")
		 .arg(_id).arg(t));

	Activity *act=new Activity(*this, _id, _activityGroupId, _teachersNames, _subjectName, _activityTagsNames,
		_studentsNames, _duration, _totalDuration, _active, _computeNTotalStudents, _nTotalStudents, _computedNumberOfStudents);

	this->activitiesList << act; //append

	assert(!activitiesPointerHash.contains(act->id));
	activitiesPointerHash.insert(act->id, act);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

bool Rules::addSplitActivity(
	QWidget* parent,
	int _firstActivityId,
	int _activityGroupId,
	const QStringList& _teachersNames,
	const QString& _subjectName,
	const QStringList& _activityTagsNames,
	const QStringList& _studentsNames,
	int _nSplits,
	int _totalDuration,
	int _durations[],
	bool _active[],
	int _minDayDistance,
	double _weightPercentage,
	bool _consecutiveIfSameDay,
	bool _computeNTotalStudents,
	int _nTotalStudents)
{
	//check for duplicates - idea and code by Volker Dirr
	int t=QStringList(_teachersNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activities with group_Id=%1 contain %2 duplicate teachers - please correct that")
		 .arg(_activityGroupId).arg(t));

	t=QStringList(_studentsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activities with group_Id=%1 contain %2 duplicate students sets - please correct that")
		 .arg(_activityGroupId).arg(t));

	t=QStringList(_activityTagsNames).removeDuplicates();
	if(t>0)
		RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("Activities with group_Id=%1 contain %2 duplicate activity tags - please correct that")
		 .arg(_activityGroupId).arg(t));

	assert(_firstActivityId==_activityGroupId);

	QList<int> acts;

	acts.clear();
	for(int i=0; i<_nSplits; i++){
		Activity *act=new Activity(*this, _firstActivityId+i, _activityGroupId,
		 _teachersNames, _subjectName, _activityTagsNames, _studentsNames,
		 _durations[i], _totalDuration, _active[i], _computeNTotalStudents, _nTotalStudents);

		this->activitiesList << act; //append

		assert(!activitiesPointerHash.contains(act->id));
		activitiesPointerHash.insert(act->id, act);

		acts.append(_firstActivityId+i);
	}

	if(_minDayDistance>0){
		TimeConstraint *constr=new ConstraintMinDaysBetweenActivities(_weightPercentage, _consecutiveIfSameDay, _nSplits, acts, _minDayDistance);
		bool tmp=this->addTimeConstraint(constr);
		assert(tmp);
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	return true;
}

void Rules::removeActivity(int _id)
{
	bool recomputeTime=false, recomputeSpace=false;

	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(_id==act->id){
			if(groupActivitiesInInitialOrderList.count()>0){
				for(int i=0; i<groupActivitiesInInitialOrderList.count(); i++){
					GroupActivitiesInInitialOrderItem& item=groupActivitiesInInitialOrderList[i];
					if(item.ids.contains(act->id)){
						int t=item.ids.removeAll(act->id);
						assert(t==1);
						if(item.ids.count()<2){
							groupActivitiesInInitialOrderList.removeAt(i);
						}
						break; //The activity must be used only once in all items.
					}
				}
			}

			assert(activitiesPointerHash.contains(act->id));
			activitiesPointerHash.remove(act->id);
			
			if(mdbaHash.contains(act->id)){
				int t=mdbaHash.remove(act->id);
				assert(t==1);
			}
		
			for(int j=0; j<this->timeConstraintsList.size(); ){
				TimeConstraint* ctr=this->timeConstraintsList[j];
				if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
					ConstraintActivityPreferredStartingTime *apt=(ConstraintActivityPreferredStartingTime*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
						recomputeTime=true;
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_CONSECUTIVE){
					ConstraintTwoActivitiesConsecutive *apt=(ConstraintTwoActivitiesConsecutive*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_GROUPED){
					ConstraintTwoActivitiesGrouped *apt=(ConstraintTwoActivitiesGrouped*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_THREE_ACTIVITIES_GROUPED){
					ConstraintThreeActivitiesGrouped *apt=(ConstraintThreeActivitiesGrouped*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->thirdActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_ORDERED){
					ConstraintTwoActivitiesOrdered *apt=(ConstraintTwoActivitiesOrdered*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_TIME_SLOTS){
					ConstraintActivityPreferredTimeSlots *apt=(ConstraintActivityPreferredTimeSlots*)ctr;
					if(apt->p_activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIMES){
					ConstraintActivityPreferredStartingTimes *apt=(ConstraintActivityPreferredStartingTimes*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_ENDS_STUDENTS_DAY){
					ConstraintActivityEndsStudentsDay *apt=(ConstraintActivityEndsStudentsDay*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else
					j++;
			}

			for(int j=0; j<this->spaceConstraintsList.size(); ){
				SpaceConstraint* ctr=this->spaceConstraintsList[j];
				if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
					if(((ConstraintActivityPreferredRoom*)ctr)->activityId==act->id){
						this->removeSpaceConstraint(ctr);
						recomputeSpace=true;
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOMS){
					if(((ConstraintActivityPreferredRooms*)ctr)->activityId==act->id)
						this->removeSpaceConstraint(ctr);
					else
						j++;
				}
				else
					j++;
			}

			//remove the activity
			delete this->activitiesList[i];
			this->activitiesList.removeAt(i); 
			break;
		}
	}

	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
			((ConstraintMinDaysBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMinDaysBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_OCCUPY_MAX_TIME_SLOTS_FROM_SELECTION){
			((ConstraintActivitiesOccupyMaxTimeSlotsFromSelection*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesOccupyMaxTimeSlotsFromSelection*)ctr)->activitiesIds.count()<1)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_MAX_SIMULTANEOUS_IN_SELECTED_TIME_SLOTS){
			((ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots*)ctr)->activitiesIds.count()<1)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_MAX_DAYS_BETWEEN_ACTIVITIES){
			((ConstraintMaxDaysBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMaxDaysBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_MIN_GAPS_BETWEEN_ACTIVITIES){
			((ConstraintMinGapsBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMinGapsBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_TIME){
			((ConstraintActivitiesSameStartingTime*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingTime*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_HOUR){
			((ConstraintActivitiesSameStartingHour*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingHour*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_DAY){
			((ConstraintActivitiesSameStartingDay*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingDay*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_NOT_OVERLAPPING){
			((ConstraintActivitiesNotOverlapping*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesNotOverlapping*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}
	
	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
		if(ctr->type==CONSTRAINT_ACTIVITIES_OCCUPY_MAX_DIFFERENT_ROOMS){
			((ConstraintActivitiesOccupyMaxDifferentRooms*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesOccupyMaxDifferentRooms*)ctr)->activitiesIds.count()<2)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_ROOM_IF_CONSECUTIVE){
			((ConstraintActivitiesSameRoomIfConsecutive*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameRoomIfConsecutive*)ctr)->activitiesIds.count()<2)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}

	if(recomputeTime){
		LockUnlock::computeLockedUnlockedActivitiesOnlyTime();
	}
	if(recomputeSpace){
		LockUnlock::computeLockedUnlockedActivitiesOnlySpace();
	}
	if(recomputeTime || recomputeSpace){
		LockUnlock::increaseCommunicationSpinBox();
	}
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

void Rules::removeActivity(int _id, int _activityGroupId)
{
	bool recomputeTime=false;
	bool recomputeSpace=false;
	
	for(int i=0; i<this->activitiesList.size(); ){
		Activity* act=this->activitiesList[i];

		if(_id==act->id || (_activityGroupId>0 && _activityGroupId==act->activityGroupId)){
			if(groupActivitiesInInitialOrderList.count()>0){
				for(int i=0; i<groupActivitiesInInitialOrderList.count(); i++){
					GroupActivitiesInInitialOrderItem& item=groupActivitiesInInitialOrderList[i];
					if(item.ids.contains(act->id)){
						int t=item.ids.removeAll(act->id);
						assert(t==1);
						if(item.ids.count()<2){
							groupActivitiesInInitialOrderList.removeAt(i);
						}
						break; //The activity must be used only once in all items.
					}
				}
			}

			assert(activitiesPointerHash.contains(act->id));
			activitiesPointerHash.remove(act->id);

			if(mdbaHash.contains(act->id)){
				int t=mdbaHash.remove(act->id);
				assert(t==1);
			}
		
			for(int j=0; j<this->timeConstraintsList.size(); ){
				TimeConstraint* ctr=this->timeConstraintsList[j];
				if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
					ConstraintActivityPreferredStartingTime *apt=(ConstraintActivityPreferredStartingTime*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
						recomputeTime=true;
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_CONSECUTIVE){
					ConstraintTwoActivitiesConsecutive *apt=(ConstraintTwoActivitiesConsecutive*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_GROUPED){
					ConstraintTwoActivitiesGrouped *apt=(ConstraintTwoActivitiesGrouped*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_THREE_ACTIVITIES_GROUPED){
					ConstraintThreeActivitiesGrouped *apt=(ConstraintThreeActivitiesGrouped*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->thirdActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_TWO_ACTIVITIES_ORDERED){
					ConstraintTwoActivitiesOrdered *apt=(ConstraintTwoActivitiesOrdered*)ctr;
					if(apt->firstActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else if(apt->secondActivityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_TIME_SLOTS){
					ConstraintActivityPreferredTimeSlots *apt=(ConstraintActivityPreferredTimeSlots*)ctr;
					if(apt->p_activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIMES){
					ConstraintActivityPreferredStartingTimes *apt=(ConstraintActivityPreferredStartingTimes*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_ENDS_STUDENTS_DAY){
					ConstraintActivityEndsStudentsDay *apt=(ConstraintActivityEndsStudentsDay*)ctr;
					if(apt->activityId==act->id){
						this->removeTimeConstraint(ctr);
					}
					else
						j++;
				}
				else
					j++;
			}
			for(int j=0; j<this->spaceConstraintsList.size(); ){
				SpaceConstraint* ctr=this->spaceConstraintsList[j];
				if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
					if(((ConstraintActivityPreferredRoom*)ctr)->activityId==act->id){
						this->removeSpaceConstraint(ctr);
						recomputeSpace=true;
					}
					else
						j++;
				}
				else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOMS){
					if(((ConstraintActivityPreferredRooms*)ctr)->activityId==act->id)
						this->removeSpaceConstraint(ctr);
					else
						j++;
				}
				else
					j++;
			}

			delete this->activitiesList[i];
			this->activitiesList.removeAt(i); //if this is the last activity, then we will make one more comparison above
		}
		else
			i++;
	}

	for(int i=0; i<this->timeConstraintsList.size(); ){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
			((ConstraintMinDaysBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMinDaysBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_OCCUPY_MAX_TIME_SLOTS_FROM_SELECTION){
			((ConstraintActivitiesOccupyMaxTimeSlotsFromSelection*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesOccupyMaxTimeSlotsFromSelection*)ctr)->activitiesIds.count()<1)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_MAX_SIMULTANEOUS_IN_SELECTED_TIME_SLOTS){
			((ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots*)ctr)->activitiesIds.count()<1)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_MAX_DAYS_BETWEEN_ACTIVITIES){
			((ConstraintMaxDaysBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMaxDaysBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_MIN_GAPS_BETWEEN_ACTIVITIES){
			((ConstraintMinGapsBetweenActivities*)ctr)->removeUseless(*this);
			if(((ConstraintMinGapsBetweenActivities*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_TIME){
			((ConstraintActivitiesSameStartingTime*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingTime*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_HOUR){
			((ConstraintActivitiesSameStartingHour*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingHour*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_STARTING_DAY){
			((ConstraintActivitiesSameStartingDay*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameStartingDay*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_NOT_OVERLAPPING){
			((ConstraintActivitiesNotOverlapping*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesNotOverlapping*)ctr)->n_activities<2)
				this->removeTimeConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}

	for(int i=0; i<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
		if(ctr->type==CONSTRAINT_ACTIVITIES_OCCUPY_MAX_DIFFERENT_ROOMS){
			((ConstraintActivitiesOccupyMaxDifferentRooms*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesOccupyMaxDifferentRooms*)ctr)->activitiesIds.count()<2)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITIES_SAME_ROOM_IF_CONSECUTIVE){
			((ConstraintActivitiesSameRoomIfConsecutive*)ctr)->removeUseless(*this);
			if(((ConstraintActivitiesSameRoomIfConsecutive*)ctr)->activitiesIds.count()<2)
				this->removeSpaceConstraint(ctr);
			else
				i++;
		}
		else
			i++;
	}

	if(recomputeTime){
		LockUnlock::computeLockedUnlockedActivitiesOnlyTime();
	}
	if(recomputeSpace){
		LockUnlock::computeLockedUnlockedActivitiesOnlySpace();
	}
	if(recomputeTime || recomputeSpace){
		LockUnlock::increaseCommunicationSpinBox();
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

void Rules::modifyActivity(
	int _id,
	int _activityGroupId,
	const QStringList& _teachersNames,
	const QString& _subjectName,
	const QStringList& _activityTagsNames,
	const QStringList& _studentsNames,
	//int _nTotalStudents,
	int _nSplits,
	int _totalDuration,
	int _durations[],
	//int _parities[],
	bool _active[],
	bool _computeNTotalStudents,
	int _nTotalStudents)
{
	int i=0;
	for(int j=0; j<this->activitiesList.size(); j++){
		Activity* act=this->activitiesList[j];
		if((_activityGroupId==0 && act->id==_id) || (_activityGroupId!=0 && act->activityGroupId==_activityGroupId)){
			act->teachersNames=_teachersNames;
			act->subjectName=_subjectName;
			act->activityTagsNames=_activityTagsNames;
			act->studentsNames=_studentsNames;
			act->duration=_durations[i];
			//act->parity=_parities[i];
			act->active=_active[i];
			act->totalDuration=_totalDuration;
			act->computeNTotalStudents=_computeNTotalStudents;
			act->nTotalStudents=_nTotalStudents;
			i++;
		}
	}
		
	assert(i==_nSplits);
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

void Rules::modifySubactivity(
	int _id,
	int _activityGroupId,
	const QStringList& _teachersNames,
	const QString& _subjectName,
	const QStringList& _activityTagsNames,
	const QStringList& _studentsNames,
	int _duration,
	bool _active,
	bool _computeNTotalStudents,
	int _nTotalStudents)
{
	QList<Activity*> actsList;
	Activity* crtAct=NULL;
	
	foreach(Activity* act, this->activitiesList){
		if(act->id==_id && act->activityGroupId==_activityGroupId){
			crtAct=act;
			//actsList.append(act);
		}
		else if(act->activityGroupId!=0 && _activityGroupId!=0 && act->activityGroupId==_activityGroupId){
			actsList.append(act);
		}
	}
	
	assert(crtAct!=NULL);
	
	int td=0;
	foreach(Activity* act, actsList)
		td+=act->duration;
	td+=_duration; //crtAct->duration;
	foreach(Activity* act, actsList)
		act->totalDuration=td;

	crtAct->teachersNames=_teachersNames;
	crtAct->subjectName=_subjectName;
	crtAct->activityTagsNames=_activityTagsNames;
	crtAct->studentsNames=_studentsNames;
	crtAct->duration=_duration;
	crtAct->totalDuration=td;
	crtAct->active=_active;
	crtAct->computeNTotalStudents=_computeNTotalStudents;
	crtAct->nTotalStudents=_nTotalStudents;
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addRoom(Room* rm)
{
	if(this->searchRoom(rm->name) >= 0)
		return false;
	this->roomsList << rm; //append
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::addRoomFast(Room* rm)
{
	this->roomsList << rm; //append
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

int Rules::searchRoom(const QString& roomName)
{
	for(int i=0; i<this->roomsList.size(); i++)
		if(this->roomsList[i]->name==roomName)
			return i;
	
	return -1;
}

bool Rules::removeRoom(const QString& roomName)
{
	bool recomputeSpace=false;

	int i=this->searchRoom(roomName);
	if(i<0)
		return false;

	Room* searchedRoom=this->roomsList[i];
	assert(searchedRoom->name==roomName);

	for(int j=0; j<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[j];
		if(ctr->type==CONSTRAINT_ROOM_NOT_AVAILABLE_TIMES){
			ConstraintRoomNotAvailableTimes* crna=(ConstraintRoomNotAvailableTimes*)ctr;
			if(crna->room==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
			ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*)ctr;
			if(c->roomName==roomName){
				this->removeSpaceConstraint(ctr);
				
				recomputeSpace=true;
			}
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOMS){
			ConstraintActivityPreferredRooms* c=(ConstraintActivityPreferredRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintActivityPreferredRoom* c2=new ConstraintActivityPreferredRoom
				 (c->weightPercentage, c->activityId, c->roomsNames.at(0), true); //true means permanently locked

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
				
				recomputeSpace=true;
			}*/
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* c=(ConstraintStudentsSetHomeRoom*)ctr;
			if(c->roomName==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* c=(ConstraintStudentsSetHomeRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintStudentsSetHomeRoom* c2=new ConstraintStudentsSetHomeRoom
				 (c->weightPercentage, c->studentsName, c->roomsNames.at(0));

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
			}*/
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOM){
			ConstraintTeacherHomeRoom* c=(ConstraintTeacherHomeRoom*)ctr;
			if(c->roomName==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOMS){
			ConstraintTeacherHomeRooms* c=(ConstraintTeacherHomeRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintTeacherHomeRoom* c2=new ConstraintTeacherHomeRoom
				 (c->weightPercentage, c->teacherName, c->roomsNames.at(0));

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
			}*/
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOM){
			ConstraintSubjectPreferredRoom* c=(ConstraintSubjectPreferredRoom*)ctr;
			if(c->roomName==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOMS){
			ConstraintSubjectPreferredRooms* c=(ConstraintSubjectPreferredRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintSubjectPreferredRoom* c2=new ConstraintSubjectPreferredRoom
				 (c->weightPercentage, c->subjectName, c->roomsNames.at(0));

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
			}*/
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;
			if(c->roomName==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintSubjectActivityTagPreferredRoom* c2=new ConstraintSubjectActivityTagPreferredRoom
				 (c->weightPercentage, c->subjectName, c->activityTagName, c->roomsNames.at(0));

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
			}*/
			else
				j++;
		}

		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintActivityTagPreferredRoom* c=(ConstraintActivityTagPreferredRoom*)ctr;
			if(c->roomName==roomName)
				this->removeSpaceConstraint(ctr);
			else
				j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintActivityTagPreferredRooms* c=(ConstraintActivityTagPreferredRooms*)ctr;
			int t=c->roomsNames.removeAll(roomName);
			assert(t<=1);
			if(t==1 && c->roomsNames.count()==0)
				this->removeSpaceConstraint(ctr);
			/*else if(t==1 && c->roomsNames.count()==1){
				ConstraintActivityTagPreferredRoom* c2=new ConstraintActivityTagPreferredRoom
				 (c->weightPercentage, c->activityTagName, c->roomsNames.at(0));

				RulesUsualInformation::information(parent, tr("FET information"), 
				 tr("The constraint\n%1 will be modified into constraint\n%2 because"
				 " there is only one room left in the constraint")
				 .arg(c->getDetailedDescription(*this))
				 .arg(c2->getDetailedDescription(*this)));

				this->removeSpaceConstraint(ctr);
				this->addSpaceConstraint(c2);
			}*/
			else
				j++;
		}
		else
			j++;
	}

	delete this->roomsList[i];
	this->roomsList.removeAt(i);

	if(recomputeSpace){
		LockUnlock::computeLockedUnlockedActivitiesOnlySpace();
		LockUnlock::increaseCommunicationSpinBox();
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::modifyRoom(const QString& initialRoomName, const QString& finalRoomName, const QString& building, int capacity)
{
	int i=this->searchRoom(initialRoomName);
	if(i<0)
		return false;

	Room* searchedRoom=this->roomsList[i];
	assert(searchedRoom->name==initialRoomName);

	for(int j=0; j<this->spaceConstraintsList.size(); ){
		SpaceConstraint* ctr=this->spaceConstraintsList[j];
		if(ctr->type==CONSTRAINT_ROOM_NOT_AVAILABLE_TIMES){
			ConstraintRoomNotAvailableTimes* crna=(ConstraintRoomNotAvailableTimes*)ctr;
			if(crna->room==initialRoomName)
				crna->room=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
			ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOMS){
			ConstraintActivityPreferredRooms* c=(ConstraintActivityPreferredRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOM){
			ConstraintStudentsSetHomeRoom* c=(ConstraintStudentsSetHomeRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_STUDENTS_SET_HOME_ROOMS){
			ConstraintStudentsSetHomeRooms* c=(ConstraintStudentsSetHomeRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOM){
			ConstraintTeacherHomeRoom* c=(ConstraintTeacherHomeRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_TEACHER_HOME_ROOMS){
			ConstraintTeacherHomeRooms* c=(ConstraintTeacherHomeRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOM){
			ConstraintSubjectPreferredRoom* c=(ConstraintSubjectPreferredRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_PREFERRED_ROOMS){
			ConstraintSubjectPreferredRooms* c=(ConstraintSubjectPreferredRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintSubjectActivityTagPreferredRoom* c=(ConstraintSubjectActivityTagPreferredRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_SUBJECT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintSubjectActivityTagPreferredRooms* c=(ConstraintSubjectActivityTagPreferredRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}

		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOM){
			ConstraintActivityTagPreferredRoom* c=(ConstraintActivityTagPreferredRoom*)ctr;
			if(c->roomName==initialRoomName)
				c->roomName=finalRoomName;
			j++;
		}
		else if(ctr->type==CONSTRAINT_ACTIVITY_TAG_PREFERRED_ROOMS){
			ConstraintActivityTagPreferredRooms* c=(ConstraintActivityTagPreferredRooms*)ctr;
			int t=0;
			for(QStringList::Iterator it=c->roomsNames.begin(); it!=c->roomsNames.end(); it++){
				if((*it)==initialRoomName){
					*it=finalRoomName;
					t++;
				}
			}
			assert(t<=1);
			j++;
		}

		else
			j++;
	}

	searchedRoom->name=finalRoomName;
	searchedRoom->building=building;
	searchedRoom->capacity=capacity;

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

void Rules::sortRoomsAlphabetically()
{
	//qSort(this->roomsList.begin(), this->roomsList.end(), roomsAscending);
	std::stable_sort(this->roomsList.begin(), this->roomsList.end(), roomsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addBuilding(Building* bu)
{
	if(this->searchBuilding(bu->name) >= 0)
		return false;
	this->buildingsList << bu; //append
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::addBuildingFast(Building* bu)
{
	this->buildingsList << bu; //append
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

int Rules::searchBuilding(const QString& buildingName)
{
	for(int i=0; i<this->buildingsList.size(); i++)
		if(this->buildingsList[i]->name==buildingName)
			return i;
	
	return -1;
}

bool Rules::removeBuilding(const QString& buildingName)
{
	foreach(Room* rm, this->roomsList)
		if(rm->building==buildingName)
			rm->building="";

	int i=this->searchBuilding(buildingName);
	if(i<0)
		return false;

	Building* searchedBuilding=this->buildingsList[i];
	assert(searchedBuilding->name==buildingName);

	delete this->buildingsList[i];
	this->buildingsList.removeAt(i);
	
	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);

	teachers_schedule_ready=false;
	students_schedule_ready=false;
	rooms_schedule_ready=false;

	return true;
}

bool Rules::modifyBuilding(const QString& initialBuildingName, const QString& finalBuildingName)
{
	foreach(Room* rm, roomsList)
		if(rm->building==initialBuildingName)
			rm->building=finalBuildingName;

	int i=this->searchBuilding(initialBuildingName);
	if(i<0)
		return false;

	Building* searchedBuilding=this->buildingsList[i];
	assert(searchedBuilding->name==initialBuildingName);
	searchedBuilding->name=finalBuildingName;

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	return true;
}

void Rules::sortBuildingsAlphabetically()
{
	//qSort(this->buildingsList.begin(), this->buildingsList.end(), buildingsAscending);
	std::stable_sort(this->buildingsList.begin(), this->buildingsList.end(), buildingsAscending);

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
}

bool Rules::addTimeConstraint(TimeConstraint* ctr)
{
	bool ok=true;

	//TODO: improve this

	//check if this constraint is already added, for ConstraintActivityPreferredStartingTime
	if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
		ConstraintActivityPreferredStartingTime* c=(ConstraintActivityPreferredStartingTime*) ctr;
		QSet<ConstraintActivityPreferredStartingTime*> cs=apstHash.value(c->activityId, QSet<ConstraintActivityPreferredStartingTime*>());
		foreach(ConstraintActivityPreferredStartingTime* oldc, cs){
			if((*oldc)==(*c)){
				ok=false;
				break;
			}
		}

		/*int i;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME) 
				if(
				 *((ConstraintActivityPreferredStartingTime*)ctr2)
				 ==
				 *((ConstraintActivityPreferredStartingTime*)ctr)
				)
					break;
		}
				
		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}

	//check if this constraint is already added, for ConstraintMinDaysBetweenActivities
	else if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
		ConstraintMinDaysBetweenActivities* c=(ConstraintMinDaysBetweenActivities*) ctr;
		foreach(int aid, c->activitiesId){
			QSet<ConstraintMinDaysBetweenActivities*> cs=mdbaHash.value(aid, QSet<ConstraintMinDaysBetweenActivities*>());
			foreach(ConstraintMinDaysBetweenActivities* oldc, cs){
				if((*oldc)==(*c)){
					ok=false;
					break;
				}
			}
			if(!ok)
				break;
		}

		/*int i;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES)
				if(
				 *((ConstraintMinDaysBetweenActivities*)ctr2)
				 ==
				 *((ConstraintMinDaysBetweenActivities*)ctr)
				 )
					break;
		}

		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}
	
	else if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
		ConstraintStudentsSetNotAvailableTimes* c=(ConstraintStudentsSetNotAvailableTimes*) ctr;
		QSet<ConstraintStudentsSetNotAvailableTimes*> cs=ssnatHash.value(c->students, QSet<ConstraintStudentsSetNotAvailableTimes*>());
		foreach(ConstraintStudentsSetNotAvailableTimes* oldc, cs){
			if(oldc->students==c->students){
				ok=false;
				break;
			}
		}

		/*int i;
		ConstraintStudentsSetNotAvailableTimes* ssna=(ConstraintStudentsSetNotAvailableTimes*)ctr;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES) {
				ConstraintStudentsSetNotAvailableTimes* ssna2=(ConstraintStudentsSetNotAvailableTimes*)ctr2;
				if(ssna->students==ssna2->students)
					break;
			}
		}
				
		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}
	
	else if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
		ConstraintTeacherNotAvailableTimes* c=(ConstraintTeacherNotAvailableTimes*) ctr;
		QSet<ConstraintTeacherNotAvailableTimes*> cs=tnatHash.value(c->teacher, QSet<ConstraintTeacherNotAvailableTimes*>());
		foreach(ConstraintTeacherNotAvailableTimes* oldc, cs){
			if(oldc->teacher==c->teacher){
				ok=false;
				break;
			}
		}

		/*int i;
		ConstraintTeacherNotAvailableTimes* tna=(ConstraintTeacherNotAvailableTimes*)ctr;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES) {
				ConstraintTeacherNotAvailableTimes* tna2=(ConstraintTeacherNotAvailableTimes*)ctr2;
				if(tna->teacher==tna2->teacher)
					break;
			}
		}
				
		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}
	
	else if(ctr->type==CONSTRAINT_BREAK_TIMES){
		//ConstraintBreakTimes* c=(ConstraintBreakTimes*) ctr;
		QSet<ConstraintBreakTimes*> cs=btSet;
		if(cs.count()>0)
			ok=false;
		/*int i;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_BREAK_TIMES)
				break;
		}
				
		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}
	
	else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_TIME){
		//ConstraintBasicCompulsoryTime* c=(ConstraintBasicCompulsoryTime*) ctr;
		QSet<ConstraintBasicCompulsoryTime*> cs=bctSet;
		if(cs.count()>0)
			ok=false;
		/*int i;
		for(i=0; i<this->timeConstraintsList.size(); i++){
			TimeConstraint* ctr2=this->timeConstraintsList[i];
			if(ctr2->type==CONSTRAINT_BASIC_COMPULSORY_TIME)
				break;
		}
				
		if(i<this->timeConstraintsList.size())
			ok=false;*/
	}
	
	if(ok){
		this->timeConstraintsList << ctr; //append
		
		if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
			ConstraintActivityPreferredStartingTime* c=(ConstraintActivityPreferredStartingTime*) ctr;
			QSet<ConstraintActivityPreferredStartingTime*> cs=apstHash.value(c->activityId, QSet<ConstraintActivityPreferredStartingTime*>());
			assert(!cs.contains(c));
			cs.insert(c);
			apstHash.insert(c->activityId, cs);
		}

		else if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
			ConstraintMinDaysBetweenActivities* c=(ConstraintMinDaysBetweenActivities*) ctr;
			foreach(int aid, c->activitiesId){
				QSet<ConstraintMinDaysBetweenActivities*> cs=mdbaHash.value(aid, QSet<ConstraintMinDaysBetweenActivities*>());
				assert(!cs.contains(c));
				cs.insert(c);
				mdbaHash.insert(aid, cs);
			}
		}

		else if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
			ConstraintTeacherNotAvailableTimes* c=(ConstraintTeacherNotAvailableTimes*) ctr;
			QSet<ConstraintTeacherNotAvailableTimes*> cs=tnatHash.value(c->teacher, QSet<ConstraintTeacherNotAvailableTimes*>());
			assert(!cs.contains(c));
			cs.insert(c);
			tnatHash.insert(c->teacher, cs);
		}

		else if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* c=(ConstraintStudentsSetNotAvailableTimes*) ctr;
			QSet<ConstraintStudentsSetNotAvailableTimes*> cs=ssnatHash.value(c->students, QSet<ConstraintStudentsSetNotAvailableTimes*>());
			assert(!cs.contains(c));
			cs.insert(c);
			ssnatHash.insert(c->students, cs);
		}
		else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_TIME){
			ConstraintBasicCompulsoryTime* c=(ConstraintBasicCompulsoryTime*) ctr;
			QSet<ConstraintBasicCompulsoryTime*> &cs=bctSet;
			assert(!cs.contains(c));
			cs.insert(c);
		}
		else if(ctr->type==CONSTRAINT_BREAK_TIMES){
			ConstraintBreakTimes* c=(ConstraintBreakTimes*) ctr;
			QSet<ConstraintBreakTimes*> &cs=btSet;
			assert(!cs.contains(c));
			cs.insert(c);
		}

		this->internalStructureComputed=false;
		setRulesModifiedAndOtherThings(this);
		return true;
	}
	else
		return false;
}

bool Rules::removeTimeConstraint(TimeConstraint* ctr)
{
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		if(this->timeConstraintsList[i]==ctr){
			if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
				ConstraintActivityPreferredStartingTime* c=(ConstraintActivityPreferredStartingTime*) ctr;
				QSet<ConstraintActivityPreferredStartingTime*> cs=apstHash.value(c->activityId, QSet<ConstraintActivityPreferredStartingTime*>());
				assert(cs.contains(c));
				cs.remove(c);
				apstHash.insert(c->activityId, cs);
			}

			else if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
				ConstraintMinDaysBetweenActivities* c=(ConstraintMinDaysBetweenActivities*) ctr;
				foreach(int aid, c->activitiesId){
					QSet<ConstraintMinDaysBetweenActivities*> cs=mdbaHash.value(aid, QSet<ConstraintMinDaysBetweenActivities*>());
					assert(cs.contains(c));
					cs.remove(c);
					mdbaHash.insert(aid, cs);
				}
			}

			else if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
				ConstraintTeacherNotAvailableTimes* c=(ConstraintTeacherNotAvailableTimes*) ctr;
				QSet<ConstraintTeacherNotAvailableTimes*> cs=tnatHash.value(c->teacher, QSet<ConstraintTeacherNotAvailableTimes*>());
				assert(cs.contains(c));
				cs.insert(c);
				tnatHash.insert(c->teacher, cs);
			}

			else if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
				ConstraintStudentsSetNotAvailableTimes* c=(ConstraintStudentsSetNotAvailableTimes*) ctr;
				QSet<ConstraintStudentsSetNotAvailableTimes*> cs=ssnatHash.value(c->students, QSet<ConstraintStudentsSetNotAvailableTimes*>());
				assert(cs.contains(c));
				cs.remove(c);
				ssnatHash.insert(c->students, cs);
			}
			else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_TIME){
				ConstraintBasicCompulsoryTime* c=(ConstraintBasicCompulsoryTime*) ctr;
				QSet<ConstraintBasicCompulsoryTime*> &cs=bctSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
			else if(ctr->type==CONSTRAINT_BREAK_TIMES){
				ConstraintBreakTimes* c=(ConstraintBreakTimes*) ctr;
				QSet<ConstraintBreakTimes*> &cs=btSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
	
			delete ctr;
			this->timeConstraintsList.removeAt(i);
			this->internalStructureComputed=false;
			setRulesModifiedAndOtherThings(this);

			return true;
		}
	}

	return false;
}

bool Rules::removeTimeConstraints(QList<TimeConstraint*> _tcl)
{
	QSet<TimeConstraint*> _tcs=_tcl.toSet();
	QList<TimeConstraint*> remaining;

	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=timeConstraintsList[i];
		if(_tcs.contains(ctr)){
			if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIME){
				ConstraintActivityPreferredStartingTime* c=(ConstraintActivityPreferredStartingTime*) ctr;
				QSet<ConstraintActivityPreferredStartingTime*> cs=apstHash.value(c->activityId, QSet<ConstraintActivityPreferredStartingTime*>());
				assert(cs.contains(c));
				cs.remove(c);
				apstHash.insert(c->activityId, cs);
			}

			else if(ctr->type==CONSTRAINT_MIN_DAYS_BETWEEN_ACTIVITIES){
				ConstraintMinDaysBetweenActivities* c=(ConstraintMinDaysBetweenActivities*) ctr;
				foreach(int aid, c->activitiesId){
					QSet<ConstraintMinDaysBetweenActivities*> cs=mdbaHash.value(aid, QSet<ConstraintMinDaysBetweenActivities*>());
					assert(cs.contains(c));
					cs.remove(c);
					mdbaHash.insert(aid, cs);
				}
			}

			else if(ctr->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
				ConstraintTeacherNotAvailableTimes* c=(ConstraintTeacherNotAvailableTimes*) ctr;
				QSet<ConstraintTeacherNotAvailableTimes*> cs=tnatHash.value(c->teacher, QSet<ConstraintTeacherNotAvailableTimes*>());
				assert(cs.contains(c));
				cs.insert(c);
				tnatHash.insert(c->teacher, cs);
			}

			else if(ctr->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
				ConstraintStudentsSetNotAvailableTimes* c=(ConstraintStudentsSetNotAvailableTimes*) ctr;
				QSet<ConstraintStudentsSetNotAvailableTimes*> cs=ssnatHash.value(c->students, QSet<ConstraintStudentsSetNotAvailableTimes*>());
				assert(cs.contains(c));
				cs.remove(c);
				ssnatHash.insert(c->students, cs);
			}
			else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_TIME){
				ConstraintBasicCompulsoryTime* c=(ConstraintBasicCompulsoryTime*) ctr;
				QSet<ConstraintBasicCompulsoryTime*> &cs=bctSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
			else if(ctr->type==CONSTRAINT_BREAK_TIMES){
				ConstraintBreakTimes* c=(ConstraintBreakTimes*) ctr;
				QSet<ConstraintBreakTimes*> &cs=btSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
	
			//_tcs.remove(ctr);
			delete ctr;
		}
		else
			remaining.append(ctr);
	}
	
	bool ok;
	assert(timeConstraintsList.count()<=remaining.count()+_tcs.count());
	if(timeConstraintsList.count()==remaining.count()+_tcs.count())
		ok=true;
	else
		ok=false;

	if(remaining.count()!=timeConstraintsList.count()){
		timeConstraintsList=remaining;
	
		this->internalStructureComputed=false;
		setRulesModifiedAndOtherThings(this);
	}

	return ok;
}

bool Rules::addSpaceConstraint(SpaceConstraint* ctr)
{
	bool ok=true;

	//TODO: check if this constraint is already added...(if any possibility of duplicates)
	if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
		ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*) ctr;
		QSet<ConstraintActivityPreferredRoom*> cs=aprHash.value(c->activityId, QSet<ConstraintActivityPreferredRoom*>());
		foreach(ConstraintActivityPreferredRoom* oldc, cs){
			if((*oldc)==(*c)){
				ok=false;
				break;
			}
		}

		/*int i;
		for(i=0; i<this->spaceConstraintsList.size(); i++){
			SpaceConstraint* ctr2=this->spaceConstraintsList[i];
			if(ctr2->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM)
				if(
				 *((ConstraintActivityPreferredRoom*)ctr2)
				 ==
				 *((ConstraintActivityPreferredRoom*)ctr)
				)
					break;
		}
		
		if(i<this->spaceConstraintsList.size())
			ok=false;*/
	}
/*	else if(ctr->type==CONSTRAINT_ROOM_NOT_AVAILABLE_TIMES){
		int i;
		ConstraintRoomNotAvailableTimes* c=(ConstraintRoomNotAvailableTimes*)ctr;
		for(i=0; i<this->spaceConstraintsList.size(); i++){
			SpaceConstraint* ctr2=this->spaceConstraintsList[i];
			if(ctr2->type==CONSTRAINT_ROOM_NOT_AVAILABLE_TIMES){
				ConstraintRoomNotAvailableTimes* c2=(ConstraintRoomNotAvailableTimes*)ctr2;				
				if(c->room==c2->room)
					break;
			}
		}
		
		if(i<this->spaceConstraintsList.size())
			ok=false;
	}*/
	else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_SPACE){
		//ConstraintBasicCompulsorySpace* c=(ConstraintBasicCompulsorySpace*) ctr;
		QSet<ConstraintBasicCompulsorySpace*> cs=bcsSet;
		if(cs.count()>0)
			ok=false;
		/*int i;
		for(i=0; i<this->spaceConstraintsList.size(); i++){
			SpaceConstraint* ctr2=this->spaceConstraintsList[i];
			if(ctr2->type==CONSTRAINT_BASIC_COMPULSORY_SPACE)
				break;
		}
				
		if(i<this->spaceConstraintsList.size())
			ok=false;*/
	}

	if(ok){
		this->spaceConstraintsList << ctr; //append
		
		if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
			ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*) ctr;
			QSet<ConstraintActivityPreferredRoom*> cs=aprHash.value(c->activityId, QSet<ConstraintActivityPreferredRoom*>());
			assert(!cs.contains(c));
			cs.insert(c);
			aprHash.insert(c->activityId, cs);
		}
		else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_SPACE){
			ConstraintBasicCompulsorySpace* c=(ConstraintBasicCompulsorySpace*) ctr;
			QSet<ConstraintBasicCompulsorySpace*> &cs=bcsSet;
			assert(!cs.contains(c));
			cs.insert(c);
		}
		
		this->internalStructureComputed=false;
		setRulesModifiedAndOtherThings(this);
		return true;
	}
	else
		return false;
}

bool Rules::removeSpaceConstraint(SpaceConstraint* ctr)
{
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		if(this->spaceConstraintsList[i]==ctr){
			if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
				ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*) ctr;
				QSet<ConstraintActivityPreferredRoom*> cs=aprHash.value(c->activityId, QSet<ConstraintActivityPreferredRoom*>());
				assert(cs.contains(c));
				cs.remove(c);
				aprHash.insert(c->activityId, cs);
			}

			else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_SPACE){
				ConstraintBasicCompulsorySpace* c=(ConstraintBasicCompulsorySpace*) ctr;
				QSet<ConstraintBasicCompulsorySpace*> &cs=bcsSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
	
			delete ctr;
			this->spaceConstraintsList.removeAt(i);
			this->internalStructureComputed=false;
			setRulesModifiedAndOtherThings(this);

			return true;
		}
	}

	return false;
}

bool Rules::removeSpaceConstraints(QList<SpaceConstraint*> _scl)
{
	QSet<SpaceConstraint*> _scs=_scl.toSet();
	QList<SpaceConstraint*> remaining;

	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=spaceConstraintsList[i];
		if(_scs.contains(ctr)){
			if(ctr->type==CONSTRAINT_ACTIVITY_PREFERRED_ROOM){
				ConstraintActivityPreferredRoom* c=(ConstraintActivityPreferredRoom*) ctr;
				QSet<ConstraintActivityPreferredRoom*> cs=aprHash.value(c->activityId, QSet<ConstraintActivityPreferredRoom*>());
				assert(cs.contains(c));
				cs.remove(c);
				aprHash.insert(c->activityId, cs);
			}

			else if(ctr->type==CONSTRAINT_BASIC_COMPULSORY_SPACE){
				ConstraintBasicCompulsorySpace* c=(ConstraintBasicCompulsorySpace*) ctr;
				QSet<ConstraintBasicCompulsorySpace*> &cs=bcsSet;
				assert(cs.contains(c));
				cs.remove(c);
			}
	
			//_scs.remove(ctr);
			delete ctr;
		}
		else
			remaining.append(ctr);
	}

	bool ok;
	assert(spaceConstraintsList.count()<=remaining.count()+_scs.count());
	if(spaceConstraintsList.count()==remaining.count()+_scs.count())
		ok=true;
	else
		ok=false;
		
	if(remaining.count()!=spaceConstraintsList.count()){
		spaceConstraintsList=remaining;
		
		this->internalStructureComputed=false;
		setRulesModifiedAndOtherThings(this);
	}
	
	return ok;
}

bool Rules::read(QWidget* parent, const QString& fileName, bool commandLine, QString commandLineDirectory) //commandLineDirectory has trailing FILE_SEP
{
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly)){
		//cout<<"Could not open file - not existing or in use\n";
		RulesIrreconcilableMessage::warning(parent, tr("FET warning"), tr("Could not open file - not existing or in use"));
		return false;
	}
	
	xmlReaderNumberOfUnrecognizedFields=0;
	
	QXmlStreamReader xmlReader(&file);
	
	////////////////////////////////////////

	if(!commandLine){
		//logging part
		QDir dir;
		bool t=true;
		if(!dir.exists(OUTPUT_DIR+FILE_SEP+"logs"))
			t=dir.mkpath(OUTPUT_DIR+FILE_SEP+"logs");
		if(!t){
			RulesIrreconcilableMessage::warning(parent, tr("FET warning"), tr("Cannot create or use directory %1 - cannot continue").arg(QDir::toNativeSeparators(OUTPUT_DIR+FILE_SEP+"logs")));
			return false;
		}
		assert(t);
	}
	else{
		QDir dir;
		bool t=true;
		if(!dir.exists(commandLineDirectory+"logs"))
			t=dir.mkpath(commandLineDirectory+"logs");
		if(!t){
			RulesIrreconcilableMessage::warning(parent, tr("FET warning"), tr("Cannot create or use directory %1 - cannot continue").arg(QDir::toNativeSeparators(commandLineDirectory+"logs")));
			return false;
		}
		assert(t);
	}
	
	FakeString xmlReadingLog;
	xmlReadingLog="";

	QDate dat=QDate::currentDate();
	QTime tim=QTime::currentTime();
	QLocale loc(FET_LANGUAGE);
	QString sTime=loc.toString(dat, QLocale::ShortFormat)+" "+loc.toString(tim, QLocale::ShortFormat);

	QString reducedXmlLog="";
	reducedXmlLog+="Log generated by FET "+FET_VERSION+" on "+sTime+"\n\n";
	QString shortFileName=fileName.right(fileName.length()-fileName.lastIndexOf(FILE_SEP)-1);
	reducedXmlLog+="Reading file "+shortFileName+"\n";
	QFileInfo fileinfo(fileName);
	reducedXmlLog+="Complete file name, including path: "+QDir::toNativeSeparators(fileinfo.absoluteFilePath())+"\n";
	reducedXmlLog+="\n";

	QString tmp;
	if(commandLine)
		tmp=commandLineDirectory+"logs"+FILE_SEP+XML_PARSING_LOG_FILENAME;
	else
		tmp=OUTPUT_DIR+FILE_SEP+"logs"+FILE_SEP+XML_PARSING_LOG_FILENAME;
	QFile file2(tmp);
	bool canWriteLogFile=true;
	if(!file2.open(QIODevice::WriteOnly)){
		QString s=tr("FET cannot open the log file %1 for writing. This might mean that you don't"
			" have write permissions in this location. You can continue operation, but you might not be able to save the generated timetables"
			" as html files").arg(QDir::toNativeSeparators(tmp))+
			"\n\n"+tr("A solution is to remove that file (if it exists already) or set its permissions to allow writing")+
			"\n\n"+tr("Please report possible bug");
		IrreconcilableCriticalMessage::critical(parent, tr("FET critical"), s);
		canWriteLogFile=false;
	}
	QTextStream logStream;
	if(canWriteLogFile){
		logStream.setDevice(&file2);
		logStream.setCodec("UTF-8");
		logStream.setGenerateByteOrderMark(true);
	}
	
	bool okAbove3_12_17=true;
	bool version5AndAbove=false;
	bool warning=false;

	QString file_version;

	if(xmlReader.readNextStartElement()){
		xmlReadingLog+=" Found "+xmlReader.name().toString()+" tag\n";
	
		bool okfetTag;
		if(xmlReader.name()=="fet" || xmlReader.name()=="FET") //the new tag is fet, the old tag - FET
			okfetTag=true;
		else
			okfetTag=false;
	
		if(!okfetTag)
			okAbove3_12_17=false;
		else{
			assert(xmlReader.isStartElement());
			assert(okAbove3_12_17==true);
			/*QDomDocumentType dt=doc.doctype();
			if(dt.isNull() || dt.name()!="FET")
				okAbove3_12_17=false;
			else*/
			int filev[3], fetv[3];
			
			assert(xmlReader.name()=="fet" || xmlReader.name()=="FET");
			QString version=xmlReader.attributes().value("version").toString();
			file_version=version;
		
/*			QDomAttr a=elem1.attributeNode("version");
			
			QString version=a.value();
			file_version=version;*/

			QRegExp fileVerReCap("^(\\d+)\\.(\\d+)\\.(\\d+)(.*)$");

			int tfile=fileVerReCap.indexIn(file_version);
			filev[0]=filev[1]=filev[2]=-1;
			if(tfile!=0){
				RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("File contains a version numbering scheme which"
				" is not matched by v.v.va (3 numbers separated by points, followed by any string a, which may be empty). File will be opened, but you are adviced"
				" to check the version of the .fet file (in the beginning of the file). If this is a FET bug, please report it")+"\n\n"+
				tr("If you are opening a file older than FET format version 5, it will be converted to latest FET data format"));
				if(VERBOSE){
					cout<<"Opened file version not matched by regexp"<<endl;
				}
			}
			else{
				bool ok;
				filev[0]=fileVerReCap.cap(1).toInt(&ok);
				assert(ok);
				filev[1]=fileVerReCap.cap(2).toInt(&ok);
				assert(ok);
				filev[2]=fileVerReCap.cap(3).toInt(&ok);
				assert(ok);
				if(VERBOSE){
					cout<<"Opened file version matched by regexp: major="<<filev[0]<<", minor="<<filev[1]<<", revision="<<filev[2];
					cout<<", additional text="<<qPrintable(fileVerReCap.cap(4))<<"."<<endl;
				}
			}
		
			QRegExp fetVerReCap("^(\\d+)\\.(\\d+)\\.(\\d+)(.*)$");

			int tfet=fetVerReCap.indexIn(FET_VERSION);
			fetv[0]=fetv[1]=fetv[2]=-1;
			if(tfet!=0){
				RulesReconcilableMessage::warning(parent, tr("FET warning"), tr("FET version does not respect the format v.v.va"
				" (3 numbers separated by points, followed by any string a, which may be empty). This is probably a bug in FET - please report it"));
				if(VERBOSE){
					cout<<"FET version not matched by regexp"<<endl;
				}
			}
			else{
				bool ok;
				fetv[0]=fetVerReCap.cap(1).toInt(&ok);
				assert(ok);
				fetv[1]=fetVerReCap.cap(2).toInt(&ok);
				assert(ok);
				fetv[2]=fetVerReCap.cap(3).toInt(&ok);
				assert(ok);
				if(VERBOSE){
					cout<<"FET version matched by regexp: major="<<fetv[0]<<", minor="<<fetv[1]<<", revision="<<fetv[2];
					cout<<", additional text="<<qPrintable(fetVerReCap.cap(4))<<"."<<endl;
				}
			}
			
			if(filev[0]>=0 && fetv[0]>=0 && filev[1]>=0 && fetv[1]>=0 && filev[2]>=0 && fetv[2]>=0){
				if(filev[0]>fetv[0] || (filev[0]==fetv[0] && filev[1]>fetv[1]) || (filev[0]==fetv[0]&&filev[1]==fetv[1]&&filev[2]>fetv[2])){
					warning=true;
				}
			}
		
			if(filev[0]>=5 || (filev[0]==-1 && filev[1]==-1 && filev[2]==-1))
			//if major is >= 5 or major cannot be read
				version5AndAbove=true;
		}
	}
	if(!okAbove3_12_17){
		if(VERBOSE){
			cout<<"Invalid fet 3.12.17 or above"<<endl;
		}
		file2.close();
		RulesIrreconcilableMessage::warning(parent, tr("FET warning"), tr("File does not have a corresponding beginning tag - it should be %1 or %2. File is incorrect..."
			"it cannot be opened").arg("fet").arg("FET"));
		return false;
	}
	
	if(!version5AndAbove){
		RulesReconcilableMessage::warning(parent, tr("FET information"),
		 tr("Opening older file - it will be converted to latest format, automatically "
		 "assigning weight percentages to constraints and dropping parity for activities. "
		 "You are adviced to make a backup of your old file before saving in new format.\n\n"
		 "Please note that the default weight percentage of constraints min days between activities "
		 "will be 95% (mainly satisfied, not always) and 'force consecutive if same day' will be set to true "
		 "(meaning that if the activities are in the same day, they will be placed continuously, in a bigger duration activity). "
		 "If you want, you can modify this percent to be 100%, manually in the fet input file "
		 "or from the interface"));
	}
	
	if(warning){
		RulesReconcilableMessage::warning(parent, tr("FET information"), 
		 tr("Opening a file generated with a newer version than your current FET software ... file will be opened but it is recommended to update your FET software to the latest version")
		 +"\n\n"+tr("Your FET version: %1, file version: %2").arg(FET_VERSION).arg(file_version));
	}
	
	//Clear old rules, initialize new rules
	if(this->initialized)
		this->kill();
	this->init();
	
	this->nHoursPerDay=12;
	this->hoursOfTheDay[0]="08:00";
	this->hoursOfTheDay[1]="09:00";
	this->hoursOfTheDay[2]="10:00";
	this->hoursOfTheDay[3]="11:00";
	this->hoursOfTheDay[4]="12:00";
	this->hoursOfTheDay[5]="13:00";
	this->hoursOfTheDay[6]="14:00";
	this->hoursOfTheDay[7]="15:00";
	this->hoursOfTheDay[8]="16:00";
	this->hoursOfTheDay[9]="17:00";
	this->hoursOfTheDay[10]="18:00";
	this->hoursOfTheDay[11]="19:00";
	//this->hoursOfTheDay[12]="20:00";

	this->nDaysPerWeek=5;
	this->daysOfTheWeek[0] = tr("Monday");
	this->daysOfTheWeek[1] = tr("Tuesday");
	this->daysOfTheWeek[2] = tr("Wednesday");
	this->daysOfTheWeek[3] = tr("Thursday");
	this->daysOfTheWeek[4] = tr("Friday");

	this->institutionName=tr("Default institution");
	this->comments=tr("Default comments");

	bool skipDeprecatedConstraints=false;
	
	bool skipDuplicatedStudentsSets=false;
	
	assert(xmlReader.isStartElement() && (xmlReader.name()=="fet" || xmlReader.name()=="FET"));
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="  Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Institution_Name"){
			QString text=xmlReader.readElementText();
			this->institutionName=text;
			xmlReadingLog+="  Found institution name="+this->institutionName+"\n";
			reducedXmlLog+="Read institution name="+this->institutionName+"\n";
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			this->comments=text;
			xmlReadingLog+="  Found comments="+this->comments+"\n";
			reducedXmlLog+="Read comments="+this->comments+"\n";
		}
		else if(xmlReader.name()=="Hours_List"){
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Number"){
					QString text=xmlReader.readElementText();
					this->nHoursPerDay=text.toInt();
					xmlReadingLog+="   Found the number of hours per day = "+
					 CustomFETString::number(this->nHoursPerDay)+"\n";
					reducedXmlLog+="Added "+
					 CustomFETString::number(this->nHoursPerDay)+" hours per day\n";
					if(nHoursPerDay<=0)
						xmlReader.raiseError(tr("%1 is incorrect").arg("Number"));
					else
						assert(this->nHoursPerDay>0);
				}
				else if(xmlReader.name()=="Name"){
					QString text=xmlReader.readElementText();
					this->hoursOfTheDay[tmp]=text;
					xmlReadingLog+="   Found hour "+this->hoursOfTheDay[tmp]+"\n";
					tmp++;
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==nHoursPerDay || tmp==nHoursPerDay+1))
				xmlReader.raiseError(tr("%1: %2 and the number of %3 read do not correspond").arg("Hours_List").arg("Number").arg("Name"));
			else
				assert(tmp==nHoursPerDay || tmp==nHoursPerDay+1);
			//don't do assert tmp == nHoursPerDay, because some older files contain also the end of day hour, so tmp==nHoursPerDay+1 in this case
		}
		else if(xmlReader.name()=="Days_List"){
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Number"){
					QString text=xmlReader.readElementText();
					this->nDaysPerWeek=text.toInt();
					xmlReadingLog+="   Found the number of days per week = "+
					 CustomFETString::number(this->nDaysPerWeek)+"\n";
					reducedXmlLog+="Added "+
					 CustomFETString::number(this->nDaysPerWeek)+" days per week\n";
					if(nDaysPerWeek<=0)
						xmlReader.raiseError(tr("%1 is incorrect").arg("Number"));
					else
						assert(this->nDaysPerWeek>0);
				}
				else if(xmlReader.name()=="Name"){
					QString text=xmlReader.readElementText();
					this->daysOfTheWeek[tmp]=text;
					xmlReadingLog+="   Found day "+this->daysOfTheWeek[tmp]+"\n";
					tmp++;
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!tmp==nDaysPerWeek)
				xmlReader.raiseError(tr("%1: %2 and the number of %3 read do not correspond").arg("Days_List").arg("Number").arg("Name"));
			else
				assert(tmp==this->nDaysPerWeek);
		}
		else if(xmlReader.name()=="Teachers_List"){
			QSet<QString> teachersRead;
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Teacher"){
					Teacher* teacher=new Teacher();

					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							teacher->name=text;
							xmlReadingLog+="    Read teacher name: "+teacher->name+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					bool tmp2=teachersRead.contains(teacher->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate teacher %1 found - ignoring").arg(teacher->name));
						xmlReadingLog+="   Teacher not added - duplicate\n";
					}
					else{
						teachersRead.insert(teacher->name);
						this->addTeacherFast(teacher);
						tmp++;
						xmlReadingLog+="   Teacher added\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->teachersList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Teachers_List"));
			else{
				assert(tmp==this->teachersList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" teachers\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" teachers\n";
			}
		}
		else if(xmlReader.name()=="Subjects_List"){
			QSet<QString> subjectsRead;
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Subject"){
					Subject* subject=new Subject();
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							subject->name=text;
							xmlReadingLog+="    Read subject name: "+subject->name+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					bool tmp2=subjectsRead.contains(subject->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate subject %1 found - ignoring").arg(subject->name));
						xmlReadingLog+="   Subject not added - duplicate\n";
					}
					else{
						subjectsRead.insert(subject->name);
						this->addSubjectFast(subject);
						tmp++;
						xmlReadingLog+="   Subject inserted\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->subjectsList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Subjects_List"));
			else{
				assert(tmp==this->subjectsList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" subjects\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" subjects\n";
			}
		}
		else if(xmlReader.name()=="Subject_Tags_List"){
			QSet<QString> activityTagsRead;
		
			RulesReconcilableMessage::information(parent, tr("FET information"), tr("Your file contains subject tags list"
			  ", which is named in versions>=5.5.0 activity tags list"));
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Subject_Tag"){
					ActivityTag* activityTag=new ActivityTag();
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							activityTag->name=text;
							xmlReadingLog+="    Read activity tag name: "+activityTag->name+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					bool tmp2=activityTagsRead.contains(activityTag->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate activity tag %1 found - ignoring").arg(activityTag->name));
						xmlReadingLog+="   Activity tag not added - duplicate\n";
					}
					else{
						activityTagsRead.insert(activityTag->name);
						addActivityTagFast(activityTag);
						tmp++;
						xmlReadingLog+="   Activity tag inserted\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->activityTagsList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Subject_Tags_List"));
			else{
				assert(tmp==this->activityTagsList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" activity tags\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" activity tags\n";
			}
		}
		else if(xmlReader.name()=="Activity_Tags_List"){
			QSet<QString> activityTagsRead;
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Activity_Tag"){
					ActivityTag* activityTag=new ActivityTag();
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							activityTag->name=text;
							xmlReadingLog+="    Read activity tag name: "+activityTag->name+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					bool tmp2=activityTagsRead.contains(activityTag->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate activity tag %1 found - ignoring").arg(activityTag->name));
						xmlReadingLog+="   Activity tag not added - duplicate\n";
					}
					else{
						activityTagsRead.insert(activityTag->name);
						addActivityTagFast(activityTag);
						tmp++;
						xmlReadingLog+="   Activity tag inserted\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->activityTagsList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Activity_Tags_List"));
			else{
				assert(tmp==this->activityTagsList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" activity tags\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" activity tags\n";
			}
		}
		else if(xmlReader.name()=="Students_List"){
			QHash<QString, StudentsSet*> studentsHash;
		
			int tsgr=0;
			int tgr=0;
		
			int ny=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Year"){
					StudentsYear* sty=new StudentsYear();
					int ng=0;
					
					QSet<QString> groupsInYear;

					bool tmp2=this->addYearFast(sty);
					assert(tmp2==true);
					ny++;

					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							if(!skipDuplicatedStudentsSets){
								QString nn=text;
								//StudentsSet* ss=this->searchStudentsSet(nn);
								StudentsSet* ss=studentsHash.value(nn, NULL);
								if(ss!=NULL){
									QString str;
									
									if(ss->type==STUDENTS_YEAR)
										str=tr("Trying to add year %1, which is already added as another year - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
									else if(ss->type==STUDENTS_GROUP)
										str=tr("Trying to add year %1, which is already added as another group - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
									else if(ss->type==STUDENTS_SUBGROUP)
										str=tr("Trying to add year %1, which is already added as another subgroup - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
								
									int t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
									 tr("Skip rest"), tr("See next"), QString(),
									 1, 0 );
				 	
									if(t==0)
										skipDuplicatedStudentsSets=true;
								}
							}						
						
							sty->name=text;
							if(!studentsHash.contains(sty->name))
								studentsHash.insert(sty->name, sty);
							xmlReadingLog+="    Read year name: "+sty->name+"\n";
						}
						else if(xmlReader.name()=="Number_of_Students"){
							QString text=xmlReader.readElementText();
							sty->numberOfStudents=text.toInt();
							xmlReadingLog+="    Read year number of students: "+CustomFETString::number(sty->numberOfStudents)+"\n";
						}
						else if(xmlReader.name()=="Group"){
							StudentsGroup* stg=new StudentsGroup();
							int ns=0;

							QSet<QString> subgroupsInGroup;
							
							/*bool tmp4=this->addGroupFast(sty, stg);
							assert(tmp4==true);
							ng++;*/

							assert(xmlReader.isStartElement());
							while(xmlReader.readNextStartElement()){
								xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
								if(xmlReader.name()=="Name"){
									QString text=xmlReader.readElementText();
									if(!skipDuplicatedStudentsSets){
										QString nn=text;
										StudentsSet* ss=studentsHash.value(nn, NULL);
										if(ss!=NULL){
											QString str;
									
											if(ss->type==STUDENTS_YEAR)
												str=tr("Trying to add group %1, which is already added as another year - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
											else if(ss->type==STUDENTS_GROUP){
												if(groupsInYear.contains(nn)){
													str=tr("Trying to add group %1 in year %2 but it is already added - your file will be loaded but probably contains errors, please correct them after loading").arg(nn).arg(sty->name);
												}
												else
													str="";
											}
											else if(ss->type==STUDENTS_SUBGROUP)
												str=tr("Trying to add group %1, which is already added as another subgroup - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
								
											int t=1;
											if(str!=""){
												t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
												 tr("Skip rest"), tr("See next"), QString(),
												 1, 0 );
											}
				 	
											if(t==0)
												skipDuplicatedStudentsSets=true;
										}
									}
									
									groupsInYear.insert(text);

									if(studentsHash.contains(text)){
										delete stg;
										stg=(StudentsGroup*)(studentsHash.value(text));

										bool tmp4=this->addGroupFast(sty, stg);
										assert(tmp4==true);
										//ng++;
										xmlReader.skipCurrentElement();
										break;
									}

									bool tmp4=this->addGroupFast(sty, stg);
									assert(tmp4==true);
									ng++;

									stg->name=text;
									if(!studentsHash.contains(stg->name))
										studentsHash.insert(stg->name, stg);
									xmlReadingLog+="     Read group name: "+stg->name+"\n";
								}
								else if(xmlReader.name()=="Number_of_Students"){
									QString text=xmlReader.readElementText();
									stg->numberOfStudents=text.toInt();
									xmlReadingLog+="     Read group number of students: "+CustomFETString::number(stg->numberOfStudents)+"\n";
								}
								else if(xmlReader.name()=="Subgroup"){
									StudentsSubgroup* sts=new StudentsSubgroup();

									/*bool tmp6=this->addSubgroupFast(sty, stg, sts);
									assert(tmp6==true);
									ns++;*/

									assert(xmlReader.isStartElement());
									while(xmlReader.readNextStartElement()){
										xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
										if(xmlReader.name()=="Name"){
											QString text=xmlReader.readElementText();
											if(!skipDuplicatedStudentsSets){
												QString nn=text;
												StudentsSet* ss=studentsHash.value(nn, NULL);
												if(ss!=NULL){
													QString str;
									
													if(ss->type==STUDENTS_YEAR)
														str=tr("Trying to add subgroup %1, which is already added as another year - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
													else if(ss->type==STUDENTS_GROUP)
														str=tr("Trying to add subgroup %1, which is already added as another group - your file will be loaded but probably contains errors, please correct them after loading").arg(nn);
													else if(ss->type==STUDENTS_SUBGROUP){
														if(subgroupsInGroup.contains(nn)){
															str=tr("Trying to add subgroup %1 in year %2, group %3 but it is already added - your file will be loaded but probably contains errors, please correct them after loading").arg(nn).arg(sty->name).arg(stg->name);
														}
														else
															str="";
													}
								
													int t=1;
													if(str!=""){
														t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
														 tr("Skip rest"), tr("See next"), QString(),
														 1, 0 );
													}
						 	
													if(t==0)
														skipDuplicatedStudentsSets=true;
												}
											}
											
											subgroupsInGroup.insert(text);

											if(studentsHash.contains(text)){
												delete sts;
												sts=(StudentsSubgroup*)(studentsHash.value(text));

												bool tmp6=this->addSubgroupFast(sty, stg, sts);
												assert(tmp6==true);
												//ns++;
												xmlReader.skipCurrentElement();
												break;
											}

											bool tmp6=this->addSubgroupFast(sty, stg, sts);
											assert(tmp6==true);
											ns++;
											
											sts->name=text;
											if(!studentsHash.contains(sts->name))
												studentsHash.insert(sts->name, sts);
											xmlReadingLog+="     Read subgroup name: "+sts->name+"\n";
										}
										else if(xmlReader.name()=="Number_of_Students"){
											QString text=xmlReader.readElementText();
											sts->numberOfStudents=text.toInt();
											xmlReadingLog+="     Read subgroup number of students: "+CustomFETString::number(sts->numberOfStudents)+"\n";
										}
										else{
											xmlReader.skipCurrentElement();
											xmlReaderNumberOfUnrecognizedFields++;
										}
									}
								}
								else{
									xmlReader.skipCurrentElement();
									xmlReaderNumberOfUnrecognizedFields++;
								}
							}
							if(ns == stg->subgroupsList.size()){
								xmlReadingLog+="    Added "+CustomFETString::number(ns)+" subgroups\n";
								tsgr+=ns;
							//reducedXmlLog+="		Added "+CustomFETString::number(ns)+" subgroups\n";
							}
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					if(ng == sty->groupsList.size()){
						xmlReadingLog+="   Added "+CustomFETString::number(ng)+" groups\n";
						tgr+=ng;
						//reducedXmlLog+="	Added "+CustomFETString::number(ng)+" groups\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			xmlReadingLog+="  Added "+CustomFETString::number(ny)+" years\n";
			reducedXmlLog+="Added "+CustomFETString::number(ny)+" students years\n";
			//reducedXmlLog+="Added "+CustomFETString::number(tgr)+" students groups (see note below)\n";
			reducedXmlLog+="Added "+CustomFETString::number(tgr)+" students groups\n";
			//reducedXmlLog+="Added "+CustomFETString::number(tsgr)+" students subgroups (see note below)\n";
			reducedXmlLog+="Added "+CustomFETString::number(tsgr)+" students subgroups\n";
			assert(this->yearsList.size()==ny);

			//BEGIN test for number of students is the same in all sets with the same name
			bool reportWrongNumberOfStudents=true;
			foreach(StudentsYear* year, yearsList){
				assert(studentsHash.contains(year->name));
				StudentsSet* sy=studentsHash.value(year->name);
				if(sy->numberOfStudents!=year->numberOfStudents){
					if(reportWrongNumberOfStudents){
						QString str=tr("Minor problem found and corrected: year %1 has different number of students in two places (%2 and %3)", "%2 and %3 are number of students")
							.arg(year->name).arg(sy->numberOfStudents).arg(year->numberOfStudents)
							+
							"\n\n"+
							tr("Explanation: this is a minor problem, which appears if using overlapping students set, due to a bug in FET previous to version %1."
							" FET will now correct this problem by setting the number of students for this year, in all places where it appears,"
							" to the number that was found in the first appearance (%2). It is advisable to check the number of students for this year.")
							.arg("5.12.1").arg(sy->numberOfStudents);
						int t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
							 tr("Skip rest"), tr("See next"), QString(),
							 1, 0 );
	
						if(t==0)
							reportWrongNumberOfStudents=false;
					}
					year->numberOfStudents=sy->numberOfStudents;
				}
				
				foreach(StudentsGroup* group, year->groupsList){
					assert(studentsHash.contains(group->name));
					StudentsSet* sg=studentsHash.value(group->name);
					if(sg->numberOfStudents!=group->numberOfStudents){
						if(reportWrongNumberOfStudents){
							QString str=tr("Minor problem found and corrected: group %1 has different number of students in two places (%2 and %3)", "%2 and %3 are number of students")
								.arg(group->name).arg(sg->numberOfStudents).arg(group->numberOfStudents)
								+
								"\n\n"+
								tr("Explanation: this is a minor problem, which appears if using overlapping students set, due to a bug in FET previous to version %1."
								" FET will now correct this problem by setting the number of students for this group, in all places where it appears,"
								" to the number that was found in the first appearance (%2). It is advisable to check the number of students for this group.")
								.arg("5.12.1").arg(sg->numberOfStudents);
							int t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
								 tr("Skip rest"), tr("See next"), QString(),
								 1, 0 );
		
							if(t==0)
								reportWrongNumberOfStudents=false;
						}
						group->numberOfStudents=sg->numberOfStudents;
					}

					foreach(StudentsSubgroup* subgroup, group->subgroupsList){
						assert(studentsHash.contains(subgroup->name));
						StudentsSet* ss=studentsHash.value(subgroup->name);
						if(ss->numberOfStudents!=subgroup->numberOfStudents){
							if(reportWrongNumberOfStudents){
								QString str=tr("Minor problem found and corrected: subgroup %1 has different number of students in two places (%2 and %3)", "%2 and %3 are number of students")
									.arg(subgroup->name).arg(ss->numberOfStudents).arg(subgroup->numberOfStudents)
									+
									"\n\n"+
									tr("Explanation: this is a minor problem, which appears if using overlapping students set, due to a bug in FET previous to version %1."
									" FET will now correct this problem by setting the number of students for this subgroup, in all places where it appears,"
									" to the number that was found in the first appearance (%2). It is advisable to check the number of students for this subgroup.")
									.arg("5.12.1").arg(ss->numberOfStudents);
								int t=RulesReconcilableMessage::warning(parent, tr("FET warning"), str,
									 tr("Skip rest"), tr("See next"), QString(),
									 1, 0 );
			
								if(t==0)
									reportWrongNumberOfStudents=false;
							}
							subgroup->numberOfStudents=ss->numberOfStudents;
						}
					}
				}
			}
			//END test for number of students is the same in all sets with the same name
		}
		else if(xmlReader.name()=="Activities_List"){
			QSet<QString> allTeachers;
			QHash<QString, int> studentsSetsCount;
			QSet<QString> allSubjects;
			QSet<QString> allActivityTags;
			
			foreach(Teacher* tch, this->teachersList)
				allTeachers.insert(tch->name);

			foreach(Subject* sbj, this->subjectsList)
				allSubjects.insert(sbj->name);

			foreach(ActivityTag* at, this->activityTagsList)
				allActivityTags.insert(at->name);

			foreach(StudentsYear* year, this->yearsList){
				if(!studentsSetsCount.contains(year->name))
					studentsSetsCount.insert(year->name, year->numberOfStudents);
				else if(studentsSetsCount.value(year->name)!=year->numberOfStudents){
					//cout<<"Mistake: year "<<qPrintable(year->name)<<" appears in more places with different number of students"<<endl;
				}

				foreach(StudentsGroup* group, year->groupsList){
					if(!studentsSetsCount.contains(group->name))
						studentsSetsCount.insert(group->name, group->numberOfStudents);
					else if(studentsSetsCount.value(group->name)!=group->numberOfStudents){
						//cout<<"Mistake: group "<<qPrintable(group->name)<<" appears in more places with different number of students"<<endl;
					}
			
					foreach(StudentsSubgroup* subgroup, group->subgroupsList){
						if(!studentsSetsCount.contains(subgroup->name))
							studentsSetsCount.insert(subgroup->name, subgroup->numberOfStudents);
						else if(studentsSetsCount.value(subgroup->name)!=subgroup->numberOfStudents){
							//cout<<"Mistake: subgroup "<<qPrintable(subgroup->name)<<" appears in more places with different number of students"<<endl;
						}
					}
				}
			}

			//int nchildrennodes=elem2.childNodes().length();
			
			/*QProgressDialog progress(parent);
			progress.setLabelText(tr("Loading activities ... please wait"));
			progress.setRange(0, nchildrennodes);
			progress.setModal(true);*/
			//progress.setCancelButton(parent);
			
			//int ttt=0;
		
			int na=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
			
				/*progress.setValue(ttt);
				pqapplication->processEvents();
				if(progress.wasCanceled()){
					QMessageBox::information(parent, tr("FET information"), tr("Interrupted - only partial file was loaded"));
					return true;
				}
				
				ttt++;*/
			
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Activity"){
					bool correct=true;
				
					QString cm=QString(""); //comments
					QString tn="";
					QStringList tl;
					QString sjn="";
					QString atn="";
					QStringList atl;
					QString stn="";
					QStringList stl;
					//int p=PARITY_NOT_INITIALIZED;
					int td=-1;
					int d=-1;
					int id=-1;
					int gid=-1;
					bool ac=true;
					int nos=-1;
					bool cnos=true;
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Weekly"){
							xmlReader.skipCurrentElement();
							xmlReadingLog+="    Current activity is weekly - ignoring tag\n";
							//assert(p==PARITY_NOT_INITIALIZED);
							//p=PARITY_WEEKLY;
						}
						//old tag
						else if(xmlReader.name()=="Biweekly"){
							xmlReader.skipCurrentElement();
							xmlReadingLog+="    Current activity is fortnightly - ignoring tag\n";
							//assert(p==PARITY_NOT_INITIALIZED);
							//p=PARITY_FORTNIGHTLY;
						}
						else if(xmlReader.name()=="Fortnightly"){
							xmlReader.skipCurrentElement();
							xmlReadingLog+="    Current activity is fortnightly - ignoring tag\n";
							//assert(p==PARITY_NOT_INITIALIZED);
							//p=PARITY_FORTNIGHTLY;
						}
						else if(xmlReader.name()=="Active"){
							QString text=xmlReader.readElementText();
							if(text=="yes" || text=="true" || text=="1"){
								ac=true;
								xmlReadingLog+="	Current activity is active\n";
							}
							else{
								if(!(text=="no" || text=="false" || text=="0")){
									RulesReconcilableMessage::warning(parent, tr("FET warning"),
									 tr("Found activity active tag which is not 'true', 'false', 'yes', 'no', '1' or '0'."
									 " The activity will be considered not active",
									 "Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
								}
								//assert(text=="no" || text=="false" || text=="0");
								ac=false;
								xmlReadingLog+="	Current activity is not active\n";
							}
						}
						else if(xmlReader.name()=="Comments"){
							QString text=xmlReader.readElementText();
							cm=text;
							xmlReadingLog+="    Crt. activity comments="+cm+"\n";
						}
						else if(xmlReader.name()=="Teacher"){
							QString text=xmlReader.readElementText();
							tn=text;
							xmlReadingLog+="    Crt. activity teacher="+tn+"\n";
							tl.append(tn);
							if(!allTeachers.contains(tn))
							//if(this->searchTeacher(tn)<0)
								correct=false;
						}
						else if(xmlReader.name()=="Subject"){
							QString text=xmlReader.readElementText();
							sjn=text;
							xmlReadingLog+="    Crt. activity subject="+sjn+"\n";
							if(!allSubjects.contains(sjn))
							//if(this->searchSubject(sjn)<0)
								correct=false;
						}
						else if(xmlReader.name()=="Subject_Tag"){
							QString text=xmlReader.readElementText();
							atn=text;
							xmlReadingLog+="    Crt. activity activity_tag="+atn+"\n";
							if(atn!="")
								atl.append(atn);
							if(atn!="" && !allActivityTags.contains(atn))
							//if(atn!="" && this->searchActivityTag(atn)<0)
								correct=false;
						}
						else if(xmlReader.name()=="Activity_Tag"){
							QString text=xmlReader.readElementText();
							atn=text;
							xmlReadingLog+="    Crt. activity activity_tag="+atn+"\n";
							if(atn!="")
								atl.append(atn);
							if(atn!="" && !allActivityTags.contains(atn))
							//if(atn!="" && this->searchActivityTag(atn)<0)
								correct=false;
						}
						else if(xmlReader.name()=="Students"){
							QString text=xmlReader.readElementText();
							stn=text;
							xmlReadingLog+="    Crt. activity students+="+stn+"\n";
							stl.append(stn);
							if(!studentsSetsCount.contains(stn))
							//if(this->searchStudentsSet(stn)==NULL)
								correct=false;
						}
						else if(xmlReader.name()=="Duration"){
							QString text=xmlReader.readElementText();
							d=text.toInt();
							xmlReadingLog+="    Crt. activity duration="+CustomFETString::number(d)+"\n";
						}
						else if(xmlReader.name()=="Total_Duration"){
							QString text=xmlReader.readElementText();
							td=text.toInt();
							xmlReadingLog+="    Crt. activity total duration="+CustomFETString::number(td)+"\n";
						}
						else if(xmlReader.name()=="Id"){
							QString text=xmlReader.readElementText();
							id=text.toInt();
							xmlReadingLog+="    Crt. activity id="+CustomFETString::number(id)+"\n";
						}
						else if(xmlReader.name()=="Activity_Group_Id"){
							QString text=xmlReader.readElementText();
							gid=text.toInt();
							xmlReadingLog+="    Crt. activity group id="+CustomFETString::number(gid)+"\n";
						}
						else if(xmlReader.name()=="Number_Of_Students"){
							QString text=xmlReader.readElementText();
							cnos=false;
							nos=text.toInt();
							xmlReadingLog+="    Crt. activity number of students="+CustomFETString::number(nos)+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					if(id<0)
						xmlReader.raiseError(tr("%1 is incorrect").arg("Id"));
					else if(gid<0)
						xmlReader.raiseError(tr("%1 is incorrect").arg("Activity_Group_Id"));
					else if(d<=0)
						xmlReader.raiseError(tr("%1 is incorrect").arg("Duration"));
					else if(correct){
						assert(id>=0 && gid>=0);
						assert(d>0);
						if(td<0)
							td=d;
						
						if(cnos==true){
							assert(nos==-1);
							int _ns=0;
							foreach(QString _s, stl){
								assert(studentsSetsCount.contains(_s));
								_ns+=studentsSetsCount.value(_s);
							}
							this->addSimpleActivityRulesFast(parent, id, gid, tl, sjn, atl, stl,
								d, td, ac, cnos, nos, _ns);
						}
						else{
							this->addSimpleActivity(parent, id, gid, tl, sjn, atl, stl,
								d, td, ac, cnos, nos);
						}
						
						this->activitiesList[this->activitiesList.count()-1]->comments=cm;
						
						na++;
						xmlReadingLog+="   Added the activity\n";
					}
					else{
						xmlReader.raiseError(tr("The activity with id=%1 contains incorrect data").arg(id));
						/*xmlReadingLog+="   Activity with id ="+CustomFETString::number(id)+" contains invalid data - skipping\n";
						RulesReconcilableMessage::warning(parent, tr("FET information"),
						 tr("Activity with id=%1 contains invalid data - skipping").arg(id));*/
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			xmlReadingLog+="  Added "+CustomFETString::number(na)+" activities\n";
			reducedXmlLog+="Added "+CustomFETString::number(na)+" activities\n";
		}
		else if(xmlReader.name()=="Equipments_List"){
			RulesReconcilableMessage::warning(parent, tr("FET warning"),
			 tr("File contains deprecated equipments list - will be ignored"));
			xmlReader.skipCurrentElement();
			//NOT! xmlReaderNumberOfUnrecognizedFields++; because this entry was once allowed
		}
		else if(xmlReader.name()=="Buildings_List"){
			QSet<QString> buildingsRead;
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Building"){
					Building* bu=new Building();
					bu->name="";
					
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							bu->name=text;
							xmlReadingLog+="    Read building name: "+bu->name+"\n";
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}

					bool tmp2=buildingsRead.contains(bu->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate building %1 found - ignoring").arg(bu->name));
						xmlReadingLog+="   Building not added - duplicate\n";
					}
					else{
						buildingsRead.insert(bu->name);
						addBuildingFast(bu);
						tmp++;
						xmlReadingLog+="   Building inserted\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->buildingsList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Buildings_List"));
			else{
				assert(tmp==this->buildingsList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" buildings\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" buildings\n";
			}
		}
		else if(xmlReader.name()=="Rooms_List"){
			QSet<QString> roomsRead;
		
			int tmp=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Room"){
					Room* rm=new Room();
					rm->name="";
					rm->capacity=MAX_ROOM_CAPACITY; //infinite, if not specified
					rm->building="";
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
						if(xmlReader.name()=="Name"){
							QString text=xmlReader.readElementText();
							rm->name=text;
							xmlReadingLog+="    Read room name: "+rm->name+"\n";
						}
						else if(xmlReader.name()=="Type"){
							//rm->type=text;
							xmlReader.skipCurrentElement();
							xmlReadingLog+="    Ignoring old tag room type:\n";
						}
						else if(xmlReader.name()=="Capacity"){
							QString text=xmlReader.readElementText();
							rm->capacity=text.toInt();
							xmlReadingLog+="    Read room capacity: "+CustomFETString::number(rm->capacity)+"\n";
						}
						else if(xmlReader.name()=="Equipment"){
							//rm->addEquipment(text);
							xmlReader.skipCurrentElement();
							xmlReadingLog+="    Ignoring old tag - room equipment:\n";
						}
						else if(xmlReader.name()=="Building"){
							QString text=xmlReader.readElementText();
							rm->building=text;
							xmlReadingLog+="    Read room building:\n"+rm->building;
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					bool tmp2=roomsRead.contains(rm->name);
					if(tmp2){
						RulesReconcilableMessage::warning(parent, tr("FET warning"),
						 tr("Duplicate room %1 found - ignoring").arg(rm->name));
						xmlReadingLog+="   Room not added - duplicate\n";
					}
					else{
						roomsRead.insert(rm->name);
						addRoomFast(rm);
						tmp++;
						xmlReadingLog+="   Room inserted\n";
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			if(!(tmp==this->roomsList.size()))
				xmlReader.raiseError(tr("%1 is incorrect").arg("Rooms_List"));
			else{
				assert(tmp==this->roomsList.size());
				xmlReadingLog+="  Added "+CustomFETString::number(tmp)+" rooms\n";
				reducedXmlLog+="Added "+CustomFETString::number(tmp)+" rooms\n";
			}
		}
		else if(xmlReader.name()=="Time_Constraints_List"){
			bool reportMaxBeginningsAtSecondHourChange=true;
			bool reportMaxGapsChange=true;
			bool reportStudentsSetNotAvailableChange=true;
			bool reportTeacherNotAvailableChange=true;
			bool reportBreakChange=true;
			
			bool reportActivityPreferredTimeChange=true;
			
			bool reportActivityPreferredTimesChange=true;
			bool reportActivitiesPreferredTimesChange=true;
			
			bool reportUnspecifiedPermanentlyLockedTime=true;
			
			bool reportUnspecifiedDayOrHourPreferredStartingTime=true;
			
#if 0
			bool reportIncorrectMinDays=true;
#endif
		
			int nc=0;
			TimeConstraint *crt_constraint;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				crt_constraint=NULL;
				if(xmlReader.name()=="ConstraintBasicCompulsoryTime"){
					crt_constraint=readBasicCompulsoryTime(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherNotAvailable"){
					if(reportTeacherNotAvailableChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint teacher not available, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint teacher not available times (a matrix)."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportTeacherNotAvailableChange=false;
					}

					crt_constraint=readTeacherNotAvailable(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherNotAvailableTimes"){
					crt_constraint=readTeacherNotAvailableTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxDaysPerWeek"){
					crt_constraint=readTeacherMaxDaysPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxDaysPerWeek"){
					crt_constraint=readTeachersMaxDaysPerWeek(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintTeacherMinDaysPerWeek"){
					crt_constraint=readTeacherMinDaysPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMinDaysPerWeek"){
					crt_constraint=readTeachersMinDaysPerWeek(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintTeacherIntervalMaxDaysPerWeek"){
					crt_constraint=readTeacherIntervalMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersIntervalMaxDaysPerWeek"){
					crt_constraint=readTeachersIntervalMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxDaysPerWeek"){
					crt_constraint=readStudentsSetMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxDaysPerWeek"){
					crt_constraint=readStudentsMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetIntervalMaxDaysPerWeek"){
					crt_constraint=readStudentsSetIntervalMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsIntervalMaxDaysPerWeek"){
					crt_constraint=readStudentsIntervalMaxDaysPerWeek(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetNotAvailable"){
					if(reportStudentsSetNotAvailableChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint students set not available, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint students set not available times (a matrix)."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportStudentsSetNotAvailableChange=false;
					}

					crt_constraint=readStudentsSetNotAvailable(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetNotAvailableTimes"){
					crt_constraint=readStudentsSetNotAvailableTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintMinNDaysBetweenActivities"){
					crt_constraint=readMinNDaysBetweenActivities(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintMinDaysBetweenActivities"){
					crt_constraint=readMinDaysBetweenActivities(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintMaxDaysBetweenActivities"){
					crt_constraint=readMaxDaysBetweenActivities(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintMinGapsBetweenActivities"){
					crt_constraint=readMinGapsBetweenActivities(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesNotOverlapping"){
					crt_constraint=readActivitiesNotOverlapping(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesSameStartingTime"){
					crt_constraint=readActivitiesSameStartingTime(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesSameStartingHour"){
					crt_constraint=readActivitiesSameStartingHour(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesSameStartingDay"){
					crt_constraint=readActivitiesSameStartingDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxHoursDaily"){
					crt_constraint=readTeachersMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxHoursDaily"){
					crt_constraint=readTeacherMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxHoursContinuously"){
					crt_constraint=readTeachersMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxHoursContinuously"){
					crt_constraint=readTeacherMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherActivityTagMaxHoursContinuously"){
					crt_constraint=readTeacherActivityTagMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersActivityTagMaxHoursContinuously"){
					crt_constraint=readTeachersActivityTagMaxHoursContinuously(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintTeacherActivityTagMaxHoursDaily"){
					crt_constraint=readTeacherActivityTagMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersActivityTagMaxHoursDaily"){
					crt_constraint=readTeachersActivityTagMaxHoursDaily(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintTeachersMinHoursDaily"){
					crt_constraint=readTeachersMinHoursDaily(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMinHoursDaily"){
					crt_constraint=readTeacherMinHoursDaily(parent, xmlReader, xmlReadingLog);
				}
				else if((xmlReader.name()=="ConstraintTeachersSubgroupsMaxHoursDaily"
				 //TODO: erase the line below. It is only kept for compatibility with older versions
				 || xmlReader.name()=="ConstraintTeachersSubgroupsNoMoreThanXHoursDaily") && !skipDeprecatedConstraints){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint teachers subgroups max hours daily - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
					
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintStudentsNHoursDaily" && !skipDeprecatedConstraints){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint students n hours daily - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
					
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintStudentsSetNHoursDaily" && !skipDeprecatedConstraints){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint students set n hours daily - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
					
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxHoursDaily"){
					crt_constraint=readStudentsMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxHoursDaily"){
					crt_constraint=readStudentsSetMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxHoursContinuously"){
					crt_constraint=readStudentsMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxHoursContinuously"){
					crt_constraint=readStudentsSetMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetActivityTagMaxHoursContinuously"){
					crt_constraint=readStudentsSetActivityTagMaxHoursContinuously(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsActivityTagMaxHoursContinuously"){
					crt_constraint=readStudentsActivityTagMaxHoursContinuously(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintStudentsSetActivityTagMaxHoursDaily"){
					crt_constraint=readStudentsSetActivityTagMaxHoursDaily(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsActivityTagMaxHoursDaily"){
					crt_constraint=readStudentsActivityTagMaxHoursDaily(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintStudentsMinHoursDaily"){
					crt_constraint=readStudentsMinHoursDaily(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMinHoursDaily"){
					crt_constraint=readStudentsSetMinHoursDaily(parent, xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredTime"){
					if(reportActivityPreferredTimeChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains old constraint type activity preferred time, which will be converted"
						 " to the newer similar constraint of this type, constraint activity preferred STARTING time."
						 " This improvement is done in versions 5.5.9 and above"),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportActivityPreferredTimeChange=false;
					}
					
					crt_constraint=readActivityPreferredTime(parent, xmlReader, xmlReadingLog,
						reportUnspecifiedPermanentlyLockedTime, reportUnspecifiedDayOrHourPreferredStartingTime);
				}
				
				else if(xmlReader.name()=="ConstraintActivityPreferredStartingTime"){
					crt_constraint=readActivityPreferredStartingTime(parent, xmlReader, xmlReadingLog,
						reportUnspecifiedPermanentlyLockedTime, reportUnspecifiedDayOrHourPreferredStartingTime);
				}
				else if(xmlReader.name()=="ConstraintActivityEndsStudentsDay"){
					crt_constraint=readActivityEndsStudentsDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesEndStudentsDay"){
					crt_constraint=readActivitiesEndStudentsDay(xmlReader, xmlReadingLog);
				}
				//old, with 2 and 3
				else if(xmlReader.name()=="Constraint2ActivitiesConsecutive"){
					crt_constraint=read2ActivitiesConsecutive(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="Constraint2ActivitiesGrouped"){
					crt_constraint=read2ActivitiesGrouped(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="Constraint3ActivitiesGrouped"){
					crt_constraint=read3ActivitiesGrouped(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="Constraint2ActivitiesOrdered"){
					crt_constraint=read2ActivitiesOrdered(xmlReader, xmlReadingLog);
				}
				//end old
				else if(xmlReader.name()=="ConstraintTwoActivitiesConsecutive"){
					crt_constraint=readTwoActivitiesConsecutive(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTwoActivitiesGrouped"){
					crt_constraint=readTwoActivitiesGrouped(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintThreeActivitiesGrouped"){
					crt_constraint=readThreeActivitiesGrouped(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTwoActivitiesOrdered"){
					crt_constraint=readTwoActivitiesOrdered(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivityEndsDay" && !skipDeprecatedConstraints ){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint activity ends day - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
					
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredTimes"){
					if(reportActivityPreferredTimesChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("Your file contains old constraint activity preferred times, which will be converted to"
						 " new equivalent constraint activity preferred starting times. Beginning with FET-5.5.9 it is possible"
						 " to specify: 1. the starting times of an activity (constraint activity preferred starting times)"
						 " or: 2. the accepted time slots (constraint activity preferred time slots)."
						 " If what you need is type 2 of this constraint, you will have to add it by yourself from the interface."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportActivityPreferredTimesChange=false;
					}
					
					crt_constraint=readActivityPreferredTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredTimeSlots"){
					crt_constraint=readActivityPreferredTimeSlots(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredStartingTimes"){
					crt_constraint=readActivityPreferredStartingTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintBreak"){
					if(reportBreakChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint break, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint break times (a matrix)."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportBreakChange=false;
					}
					
					crt_constraint=readBreak(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintBreakTimes"){
					crt_constraint=readBreakTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersNoGaps"){
					crt_constraint=readTeachersNoGaps(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxGapsPerWeek"){
					crt_constraint=readTeachersMaxGapsPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxGapsPerWeek"){
					crt_constraint=readTeacherMaxGapsPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxGapsPerDay"){
					crt_constraint=readTeachersMaxGapsPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxGapsPerDay"){
					crt_constraint=readTeacherMaxGapsPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsNoGaps"){
					if(reportMaxGapsChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint students no gaps, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint students max gaps per week,"
						 " with max gaps=0. If you like, you can modify this constraint to allow"
						 " more gaps per week (normally not accepted in schools)"),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportMaxGapsChange=false;
					}
					
					crt_constraint=readStudentsNoGaps(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetNoGaps"){
					if(reportMaxGapsChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint students set no gaps, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint students set max gaps per week,"
						 " with max gaps=0. If you like, you can modify this constraint to allow"
						 " more gaps per week (normally not accepted in schools)"),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportMaxGapsChange=false;
					}
					
					crt_constraint=readStudentsSetNoGaps(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxGapsPerWeek"){
					crt_constraint=readStudentsMaxGapsPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxGapsPerWeek"){
					crt_constraint=readStudentsSetMaxGapsPerWeek(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintStudentsMaxGapsPerDay"){
					crt_constraint=readStudentsMaxGapsPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxGapsPerDay"){
					crt_constraint=readStudentsSetMaxGapsPerDay(xmlReader, xmlReadingLog);
				}

				else if(xmlReader.name()=="ConstraintStudentsEarly"){
					if(reportMaxBeginningsAtSecondHourChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint students early, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint students early max beginnings at second hour,"
						 " with max beginnings=0. If you like, you can modify this constraint to allow"
						 " more beginnings at second available hour (above 0 - this will make the timetable easier)"),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
 						if(t==0)
							reportMaxBeginningsAtSecondHourChange=false;
					}
					
					crt_constraint=readStudentsEarly(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsEarlyMaxBeginningsAtSecondHour"){
					crt_constraint=readStudentsEarlyMaxBeginningsAtSecondHour(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetEarly"){
					if(reportMaxBeginningsAtSecondHourChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint students set early, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint students set early max beginnings at second hour,"
						 " with max beginnings=0. If you like, you can modify this constraint to allow"
						 " more beginnings at second available hour (above 0 - this will make the timetable easier)"),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportMaxBeginningsAtSecondHourChange=false;
					}
					
					crt_constraint=readStudentsSetEarly(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour"){
					crt_constraint=readStudentsSetEarlyMaxBeginningsAtSecondHour(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesPreferredTimes"){
					if(reportActivitiesPreferredTimesChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("Your file contains old constraint activities preferred times, which will be converted to"
						 " new equivalent constraint activities preferred starting times. Beginning with FET-5.5.9 it is possible"
						 " to specify: 1. the starting times of several activities (constraint activities preferred starting times)"
						 " or: 2. the accepted time slots (constraint activities preferred time slots)."
						 " If what you need is type 2 of this constraint, you will have to add it by yourself from the interface."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportActivitiesPreferredTimesChange=false;
					}
					
					crt_constraint=readActivitiesPreferredTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesPreferredTimeSlots"){
					crt_constraint=readActivitiesPreferredTimeSlots(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesPreferredStartingTimes"){
					crt_constraint=readActivitiesPreferredStartingTimes(xmlReader, xmlReadingLog);
				}
////////////////
				else if(xmlReader.name()=="ConstraintSubactivitiesPreferredTimeSlots"){
					crt_constraint=readSubactivitiesPreferredTimeSlots(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubactivitiesPreferredStartingTimes"){
					crt_constraint=readSubactivitiesPreferredStartingTimes(xmlReader, xmlReadingLog);
				}
////////////////2011-09-25
				else if(xmlReader.name()=="ConstraintActivitiesOccupyMaxTimeSlotsFromSelection"){
					crt_constraint=readActivitiesOccupyMaxTimeSlotsFromSelection(xmlReader, xmlReadingLog);
				}
////////////////
////////////////2011-09-30
				else if(xmlReader.name()=="ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots"){
					crt_constraint=readActivitiesMaxSimultaneousInSelectedTimeSlots(xmlReader, xmlReadingLog);
				}
////////////////

				else if(xmlReader.name()=="ConstraintTeachersSubjectTagsMaxHoursContinuously" && !skipDeprecatedConstraints){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint teachers subject tags max hours continuously - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
													 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintTeachersSubjectTagMaxHoursContinuously" && !skipDeprecatedConstraints){
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint teachers subject tag max hours continuously - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0 );
													 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}

//corruptConstraintTime:
				//here we skip invalid constraint or add valid one
				if(crt_constraint!=NULL){
					assert(crt_constraint!=NULL);
					bool tmp=this->addTimeConstraint(crt_constraint);
					if(!tmp){
						RulesReconcilableMessage::warning(parent, tr("FET information"),
						 tr("Constraint\n%1\nnot added - must be a duplicate").
						 arg(crt_constraint->getDetailedDescription(*this)));
						delete crt_constraint;
					}
					else
						nc++;
				}
			}
			xmlReadingLog+="  Added "+CustomFETString::number(nc)+" time constraints\n";
			reducedXmlLog+="Added "+CustomFETString::number(nc)+" time constraints\n";
		}
		else if(xmlReader.name()=="Space_Constraints_List"){
			bool reportRoomNotAvailableChange=true;

			bool reportUnspecifiedPermanentlyLockedSpace=true;

			int nc=0;
			SpaceConstraint *crt_constraint;
			
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				crt_constraint=NULL;
				if(xmlReader.name()=="ConstraintBasicCompulsorySpace"){
					crt_constraint=readBasicCompulsorySpace(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintRoomNotAvailable"){
					if(reportRoomNotAvailableChange){
						int t=RulesReconcilableMessage::information(parent, tr("FET information"),
						 tr("File contains constraint room not available, which is old (it was improved in FET 5.5.0), and will be converted"
						 " to the similar constraint of this type, constraint room not available times (a matrix)."),
						  tr("Skip rest"), tr("See next"), QString(), 1, 0 );
						if(t==0)
							reportRoomNotAvailableChange=false;
					}
					
					crt_constraint=readRoomNotAvailable(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintRoomNotAvailableTimes"){
					crt_constraint=readRoomNotAvailableTimes(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintRoomTypeNotAllowedSubjects" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint room type not allowed subjects - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintSubjectRequiresEquipments" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint subject requires equipments - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
				
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintSubjectSubjectTagRequireEquipments" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint subject tag requires equipments - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintTeacherRequiresRoom" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint teacher requires room - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintTeacherSubjectRequireRoom" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint teacher subject require room - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintMinimizeNumberOfRoomsForStudents" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint minimize number of rooms for students - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintMinimizeNumberOfRoomsForTeachers" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint minimize number of rooms for teachers - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredRoom"){
					crt_constraint=readActivityPreferredRoom(parent, xmlReader, xmlReadingLog, reportUnspecifiedPermanentlyLockedSpace);
				}
				else if(xmlReader.name()=="ConstraintActivityPreferredRooms"){
					crt_constraint=readActivityPreferredRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivitiesSameRoom" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint activities same room - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintSubjectPreferredRoom"){
					crt_constraint=readSubjectPreferredRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubjectPreferredRooms"){
					crt_constraint=readSubjectPreferredRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubjectSubjectTagPreferredRoom"){
					crt_constraint=readSubjectSubjectTagPreferredRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubjectSubjectTagPreferredRooms"){
					crt_constraint=readSubjectSubjectTagPreferredRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubjectActivityTagPreferredRoom"){
					crt_constraint=readSubjectActivityTagPreferredRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintSubjectActivityTagPreferredRooms"){
					crt_constraint=readSubjectActivityTagPreferredRooms(xmlReader, xmlReadingLog);
				}
				//added 6 apr 2009
				else if(xmlReader.name()=="ConstraintActivityTagPreferredRoom"){
					crt_constraint=readActivityTagPreferredRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintActivityTagPreferredRooms"){
					crt_constraint=readActivityTagPreferredRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetHomeRoom"){
					crt_constraint=readStudentsSetHomeRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetHomeRooms"){
					crt_constraint=readStudentsSetHomeRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherHomeRoom"){
					crt_constraint=readTeacherHomeRoom(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherHomeRooms"){
					crt_constraint=readTeacherHomeRooms(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintMaxBuildingChangesPerDayForTeachers" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint max building changes per day for teachers - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintMaxBuildingChangesPerDayForStudents" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint max building changes per day for students - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintMaxRoomChangesPerDayForTeachers" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint max room changes per day for teachers - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;
					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintMaxRoomChangesPerDayForStudents" && !skipDeprecatedConstraints){
				
					int t=RulesReconcilableMessage::warning(parent, tr("FET warning"),
					 tr("File contains deprecated constraint max room changes per day for students - will be ignored\n"),
					 tr("Skip rest"), tr("See next"), QString(),
					 1, 0);
					 
					if(t==0)
						skipDeprecatedConstraints=true;

					crt_constraint=NULL;
					xmlReader.skipCurrentElement();
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxBuildingChangesPerDay"){
					crt_constraint=readTeacherMaxBuildingChangesPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxBuildingChangesPerDay"){
					crt_constraint=readTeachersMaxBuildingChangesPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMaxBuildingChangesPerWeek"){
					crt_constraint=readTeacherMaxBuildingChangesPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMaxBuildingChangesPerWeek"){
					crt_constraint=readTeachersMaxBuildingChangesPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeacherMinGapsBetweenBuildingChanges"){
					crt_constraint=readTeacherMinGapsBetweenBuildingChanges(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintTeachersMinGapsBetweenBuildingChanges"){
					crt_constraint=readTeachersMinGapsBetweenBuildingChanges(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxBuildingChangesPerDay"){
					crt_constraint=readStudentsSetMaxBuildingChangesPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxBuildingChangesPerDay"){
					crt_constraint=readStudentsMaxBuildingChangesPerDay(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMaxBuildingChangesPerWeek"){
					crt_constraint=readStudentsSetMaxBuildingChangesPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMaxBuildingChangesPerWeek"){
					crt_constraint=readStudentsMaxBuildingChangesPerWeek(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsSetMinGapsBetweenBuildingChanges"){
					crt_constraint=readStudentsSetMinGapsBetweenBuildingChanges(xmlReader, xmlReadingLog);
				}
				else if(xmlReader.name()=="ConstraintStudentsMinGapsBetweenBuildingChanges"){
					crt_constraint=readStudentsMinGapsBetweenBuildingChanges(xmlReader, xmlReadingLog);
				}
////////////////2012-04-29
				else if(xmlReader.name()=="ConstraintActivitiesOccupyMaxDifferentRooms"){
					crt_constraint=readActivitiesOccupyMaxDifferentRooms(xmlReader, xmlReadingLog);
				}
////////////////
////////////////2013-09-14
				else if(xmlReader.name()=="ConstraintActivitiesSameRoomIfConsecutive"){
					crt_constraint=readActivitiesSameRoomIfConsecutive(xmlReader, xmlReadingLog);
				}
////////////////
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}

//corruptConstraintSpace:
				//here we skip invalid constraint or add valid one
				if(crt_constraint!=NULL){
					assert(crt_constraint!=NULL);
					
					bool tmp=this->addSpaceConstraint(crt_constraint);
					if(!tmp){
						RulesReconcilableMessage::warning(parent, tr("FET information"),
						 tr("Constraint\n%1\nnot added - must be a duplicate").
						 arg(crt_constraint->getDetailedDescription(*this)));
						delete crt_constraint;
					}
					else
						nc++;
				}
			}
			xmlReadingLog+="  Added "+CustomFETString::number(nc)+" space constraints\n";
			reducedXmlLog+="Added "+CustomFETString::number(nc)+" space constraints\n";
		}
		else if(xmlReader.name()=="Timetable_Generation_Options_List"){
			int tgol=0;
			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";
				
				if(xmlReader.name()=="GroupActivitiesInInitialOrder"){
					tgol++;
					GroupActivitiesInInitialOrderItem item;
					int nActs=-1;
					assert(xmlReader.isStartElement());
					while(xmlReader.readNextStartElement()){
						xmlReadingLog+="   Found "+xmlReader.name().toString()+" tag\n";

						if(xmlReader.name()=="Number_of_Activities"){
							QString text=xmlReader.readElementText();
							nActs=text.toInt();
							xmlReadingLog+="    Read n_activities="+CustomFETString::number(nActs)+"\n";
						}
						else if(xmlReader.name()=="Activity_Id"){
							QString text=xmlReader.readElementText();
							int id=text.toInt();
							xmlReadingLog+="    Activity id="+CustomFETString::number(id)+"\n";
							item.ids.append(id);
						}
						else if(xmlReader.name()=="Active"){
							QString text=xmlReader.readElementText();
							if(text=="false"){
								item.active=false;
							}
						}
						else if(xmlReader.name()=="Comments"){
							QString text=xmlReader.readElementText();
							item.comments=text;
						}
						else{
							xmlReader.skipCurrentElement();
							xmlReaderNumberOfUnrecognizedFields++;
						}
					}
					if(!(nActs==item.ids.count())){
						xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
					}
					else{
						assert(nActs==item.ids.count());
						groupActivitiesInInitialOrderList.append(item);
					}
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			xmlReadingLog+="  Added "+CustomFETString::number(tgol)+" timetable generation options\n";
			reducedXmlLog+="Added "+CustomFETString::number(tgol)+" timetable generation options\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(xmlReader.error()){
		RulesIrreconcilableMessage::warning(parent, tr("FET warning"),
		 tr("Could not read file - XML parse error at line %1, column %2:\n%3", "The error description is %3")
		 .arg(xmlReader.lineNumber())
		 .arg(xmlReader.columnNumber())
		 .arg(xmlReader.errorString()));
	
		file.close();
		return false;
	}
	file.close();

	this->internalStructureComputed=false;
	
	/*reducedXmlLog+="\n";
	reducedXmlLog+="Note: if you have overlapping students sets (years or groups), a group or a subgroup may be counted more than once. "
		"A unique group name is counted once for each year it belongs to and a unique subgroup name is counted once for each year+group it belongs to.\n";*/

	if(xmlReaderNumberOfUnrecognizedFields>0){
		xmlReadingLog+="  Warning: There were "+CustomFETString::number(xmlReaderNumberOfUnrecognizedFields)+" unrecognized fields in the input file\n";

		reducedXmlLog+="\n";
		reducedXmlLog+="Warning: There were "+CustomFETString::number(xmlReaderNumberOfUnrecognizedFields)+" unrecognized fields in the input file\n";
	}

	if(canWriteLogFile){
		//logStream<<xmlReadingLog;
		logStream<<reducedXmlLog;
	}

	if(file2.error()>0){
		IrreconcilableCriticalMessage::critical(parent, tr("FET critical"),
		 tr("Saving of logging gave error code %1, which means you cannot see the log of reading the file. Please check your disk free space")
		 .arg(file2.error()));
	}

	if(canWriteLogFile)
		file2.close();

	////////////////////////////////////////

	return true;
}

bool Rules::write(QWidget* parent, const QString& filename)
{
	assert(this->initialized);

	//QString s;
	
	bool exists=false;
	QString filenameTmp=filename;
	if(QFile::exists(filenameTmp)){
		exists=true;

		filenameTmp.append(QString(".tmp"));
		
		if(QFile::exists(filenameTmp)){
			int i=1;
			for(;;){
				QString t2=filenameTmp+CustomFETString::number(i);
				if(!QFile::exists(t2)){
					filenameTmp=t2;
					break;
				}
				i++;
			}
		}
	}

	assert(!QFile::exists(filenameTmp));

	QFile file(filenameTmp);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)){
		IrreconcilableCriticalMessage::critical(parent, tr("FET critical"),
		 tr("Cannot open %1 for writing ... please check write permissions of the selected directory or your disk free space. Saving of file aborted").arg(QFileInfo(filenameTmp).fileName()));
		
		return false;
	}

	QTextStream tos(&file);
	
	tos.setCodec("UTF-8");
	tos.setGenerateByteOrderMark(true);
	//tos.setEncoding(QTextStream::UnicodeUTF8);
	
	tos<<"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";

//	tos<<"<!DOCTYPE FET><FET version=\""+FET_VERSION+"\">\n\n";
	tos<<"<fet version=\""+FET_VERSION+"\">\n\n";
	
	//the institution name and comments
	tos<<"<Institution_Name>"+protect(this->institutionName)+"</Institution_Name>\n\n";
	tos<<"<Comments>"+protect(this->comments)+"</Comments>\n\n";

	//the hours and days
	tos<<"<Hours_List>\n	<Number>"+CustomFETString::number(this->nHoursPerDay)+"</Number>\n";
	for(int i=0; i<this->nHoursPerDay; i++)
		tos<<"	<Name>"+protect(this->hoursOfTheDay[i])+"</Name>\n";
	tos<<"</Hours_List>\n\n";
	tos<<"<Days_List>\n	<Number>"+CustomFETString::number(this->nDaysPerWeek)+"</Number>\n";
	for(int i=0; i<this->nDaysPerWeek; i++)
		tos<<"	<Name>"+protect(this->daysOfTheWeek[i])+"</Name>\n";
	tos<<"</Days_List>\n\n";

	//students list
	tos<<"<Students_List>\n";
	for(int i=0; i<this->yearsList.size(); i++){
		StudentsYear* sty=this->yearsList[i];
		tos << sty->getXmlDescription();
	}
	tos<<"</Students_List>\n\n";

	//teachers list
	tos << "<Teachers_List>\n";
	for(int i=0; i<this->teachersList.size(); i++){
		Teacher* tch=this->teachersList[i];
		tos << tch->getXmlDescription();
	}
	tos << "</Teachers_List>\n\n";

	//subjects list
	tos << "<Subjects_List>\n";
	for(int i=0; i<this->subjectsList.size(); i++){
		Subject* sbj=this->subjectsList[i];
		tos << sbj->getXmlDescription();
	}
	tos << "</Subjects_List>\n\n";

	//activity tags list
	tos << "<Activity_Tags_List>\n";
	for(int i=0; i<this->activityTagsList.size(); i++){
		ActivityTag* stg=this->activityTagsList[i];
		tos << stg->getXmlDescription();
	}
	tos << "</Activity_Tags_List>\n\n";

	//activities list
	tos << "<Activities_List>\n";
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		tos << act->getXmlDescription(*this);
		tos << "\n";
	}
	tos << "</Activities_List>\n\n";

	//buildings list
	tos << "<Buildings_List>\n";
	for(int i=0; i<this->buildingsList.size(); i++){
		Building* bu=this->buildingsList[i];
		tos << bu->getXmlDescription();
	}
	tos << "</Buildings_List>\n\n";

	//rooms list
	tos << "<Rooms_List>\n";
	for(int i=0; i<this->roomsList.size(); i++){
		Room* rm=this->roomsList[i];
		tos << rm->getXmlDescription();
	}
	tos << "</Rooms_List>\n\n";

	//time constraints list
	tos << "<Time_Constraints_List>\n";
	for(int i=0; i<this->timeConstraintsList.size(); i++){
		TimeConstraint* ctr=this->timeConstraintsList[i];
		tos << ctr->getXmlDescription(*this);
	}
	tos << "</Time_Constraints_List>\n\n";

	//constraints list
	tos << "<Space_Constraints_List>\n";
	for(int i=0; i<this->spaceConstraintsList.size(); i++){
		SpaceConstraint* ctr=this->spaceConstraintsList[i];
		tos << ctr->getXmlDescription(*this);
	}
	tos << "</Space_Constraints_List>\n\n";
	
	if(groupActivitiesInInitialOrderList.count()>0){
		tos << "<Timetable_Generation_Options_List>\n";
		for(int i=0; i<groupActivitiesInInitialOrderList.count(); i++){
			GroupActivitiesInInitialOrderItem& item=groupActivitiesInInitialOrderList[i];
			tos << item.getXmlDescription(*this);
		}
		tos << "</Timetable_Generation_Options_List>\n\n";
	}

//	tos<<"</FET>\n";
	tos<<"</fet>\n";

	//tos<<s;
	
	if(file.error()>0){
		IrreconcilableCriticalMessage::critical(parent, tr("FET critical"),
		 tr("Saved file gave error code %1, which means saving is compromised. Please check your disk free space")
		 .arg(file.error()));
		
		file.close();
		return false;
	}
	
	file.close();
	
	if(exists){
		bool tf=QFile::remove(filename);
		assert(tf);
		tf=QFile::rename(filenameTmp, filename);
		assert(tf);
	}
	
	return true;
}

int Rules::activateTeacher(const QString& teacherName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->searchTeacher(teacherName)){
			if(!act->active)
				count++;
			act->active=true;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::activateStudents(const QString& studentsName)
{
	QSet<QString> allSets;
	
	StudentsSet* set=this->searchStudentsSet(studentsName);
	assert(set!=NULL);
	if(set->type==STUDENTS_SUBGROUP)
		allSets.insert(studentsName);
	else if(set->type==STUDENTS_GROUP){
		allSets.insert(studentsName);
		StudentsGroup* g=(StudentsGroup*)set;
		foreach(StudentsSubgroup* s, g->subgroupsList)
			allSets.insert(s->name);
	}
	else if(set->type==STUDENTS_YEAR){
		allSets.insert(studentsName);
		StudentsYear* y=(StudentsYear*)set;
		foreach(StudentsGroup* g, y->groupsList){
			allSets.insert(g->name);
			foreach(StudentsSubgroup* s, g->subgroupsList)
				allSets.insert(s->name);
		}
	}

	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(!act->active){
			foreach(QString set, act->studentsNames){
				if(allSets.contains(set)){
					count++;
					act->active=true;
					break;
				}
			}
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::activateSubject(const QString& subjectName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->subjectName==subjectName){
			if(!act->active)
				count++;
			act->active=true;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::activateActivityTag(const QString& activityTagName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->activityTagsNames.contains(activityTagName)){
			if(!act->active)
				count++;
			act->active=true;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::deactivateTeacher(const QString& teacherName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->searchTeacher(teacherName)){
			if(act->active)
				count++;
			act->active=false;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::deactivateStudents(const QString& studentsName)
{
	QSet<QString> allSets;
	
	StudentsSet* set=this->searchStudentsSet(studentsName);
	assert(set!=NULL);
	if(set->type==STUDENTS_SUBGROUP)
		allSets.insert(studentsName);
	else if(set->type==STUDENTS_GROUP){
		allSets.insert(studentsName);
		StudentsGroup* g=(StudentsGroup*)set;
		foreach(StudentsSubgroup* s, g->subgroupsList)
			allSets.insert(s->name);
	}
	else if(set->type==STUDENTS_YEAR){
		allSets.insert(studentsName);
		StudentsYear* y=(StudentsYear*)set;
		foreach(StudentsGroup* g, y->groupsList){
			allSets.insert(g->name);
			foreach(StudentsSubgroup* s, g->subgroupsList)
				allSets.insert(s->name);
		}
	}

	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->active){
			foreach(QString set, act->studentsNames){
				if(allSets.contains(set)){
					count++;
					act->active=false;
					break;
				}
			}
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::deactivateSubject(const QString& subjectName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->subjectName==subjectName){
			if(act->active)
				count++;
			act->active=false;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

int Rules::deactivateActivityTag(const QString& activityTagName)
{
	int count=0;
	for(int i=0; i<this->activitiesList.size(); i++){
		Activity* act=this->activitiesList[i];
		if(act->activityTagsNames.contains(activityTagName)){
			if(act->active)
				count++;
			act->active=false;
		}
	}

	this->internalStructureComputed=false;
	setRulesModifiedAndOtherThings(this);
	
	return count;
}

TimeConstraint* Rules::readBasicCompulsoryTime(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintBasicCompulsoryTime");
	ConstraintBasicCompulsoryTime* cn=new ConstraintBasicCompulsoryTime();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating automatic 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherNotAvailable(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherNotAvailable");

	QList<int> days;
	QList<int> hours;
	QString teacher;
	double weightPercentage=100;
	int d=-1, h1=-1, h2=-1;
	bool active=true;
	QString comments=QString("");
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			weightPercentage=customFETStrToDouble(text);
			if(weightPercentage<0){
				xmlReader.raiseError(tr("Weight percentage incorrect"));
				return NULL;
			}
			assert(weightPercentage>=0);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			comments=text;
		}
		else if(xmlReader.name()=="Day"){
			QString text=xmlReader.readElementText();
			for(d=0; d<this->nDaysPerWeek; d++)
				if(this->daysOfTheWeek[d]==text)
					break;
			if(d>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeacherNotAvailable day corrupt for teacher %1, day %2 is inexistent ... ignoring constraint")
					.arg(teacher)
					.arg(text));*/
				//cn=NULL;
				
				return NULL;
				//goto corruptConstraintTime;
			}
			assert(d<this->nDaysPerWeek);
			xmlReadingLog+="    Crt. day="+this->daysOfTheWeek[d]+"\n";
		}
		else if(xmlReader.name()=="Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1==this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is the last hour - impossible").arg(text));
				return NULL;
			}
			else if(h1>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeacherNotAvailable start hour corrupt for teacher %1, hour %2 is inexistent ... ignoring constraint")
					.arg(teacher)
					.arg(text));*/
				//cn=NULL;
				
				return NULL;
				//goto corruptConstraintTime;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Start hour="+this->hoursOfTheDay[h1]+"\n";
		}
		else if(xmlReader.name()=="End_Hour"){
			QString text=xmlReader.readElementText();
			for(h2=0; h2 < this->nHoursPerDay; h2++)
				if(this->hoursOfTheDay[h2]==text)
					break;
			if(h2==0){
				xmlReader.raiseError(tr("Hour %1 is the first hour - impossible").arg(text));
				return NULL;
			}
			else if(h2>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				return NULL;
			}
			/*if(h2<=0 || h2>this->nHoursPerDay){
				RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeacherNotAvailable end hour corrupt for teacher %1, hour %2 is inexistent ... ignoring constraint")
					.arg(teacher)
					.arg(text));
					
				return NULL;
				//goto corruptConstraintTime;
			}*/
			assert(h2>0 && h2 <= this->nHoursPerDay);
			xmlReadingLog+="    End hour="+this->hoursOfTheDay[h2]+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			teacher=text;
			xmlReadingLog+="    Read teacher name="+teacher+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	assert(weightPercentage>=0);
	if(d<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Day"));
		return NULL;
	}
	else if(h1<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Start_Hour"));
		return NULL;
	}
	else if(h2<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("End_Hour"));
		return NULL;
	}
	assert(d>=0 && h1>=0 && h2>=0);
	
	ConstraintTeacherNotAvailableTimes* cn = NULL;
	
	bool found=false;
	foreach(TimeConstraint* c, this->timeConstraintsList)
		if(c->type==CONSTRAINT_TEACHER_NOT_AVAILABLE_TIMES){
			ConstraintTeacherNotAvailableTimes* tna=(ConstraintTeacherNotAvailableTimes*) c;
			if(tna->teacher==teacher){
				found=true;
				
				for(int hh=h1; hh<h2; hh++){
					int k;
					for(k=0; k<tna->days.count(); k++)
						if(tna->days.at(k)==d && tna->hours.at(k)==hh)
							break;
					if(k==tna->days.count()){
						tna->days.append(d);
						tna->hours.append(hh);
					}
				}
				
				assert(tna->days.count()==tna->hours.count());
			}
		}
	if(!found){
		days.clear();
		hours.clear();
		for(int hh=h1; hh<h2; hh++){
			days.append(d);
			hours.append(hh);
		}
	
		cn=new ConstraintTeacherNotAvailableTimes(weightPercentage, teacher, days, hours);
		cn->active=active;
		cn->comments=comments;

		return cn;
	}
	else
		return NULL;
}

TimeConstraint* Rules::readTeacherNotAvailableTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherNotAvailableTimes");
	
	ConstraintTeacherNotAvailableTimes* cn=new ConstraintTeacherNotAvailableTimes();
	int nNotAvailableSlots=-1;
	int i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}

		else if(xmlReader.name()=="Number_of_Not_Available_Times"){
			QString text=xmlReader.readElementText();
			nNotAvailableSlots=text.toInt();
			xmlReadingLog+="    Read number of not available times="+CustomFETString::number(nNotAvailableSlots)+"\n";
		}

		else if(xmlReader.name()=="Not_Available_Time"){
			xmlReadingLog+="    Read: not available time\n";
			
			int d=-1;
			int h=-1;

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Day"){
					QString text=xmlReader.readElementText();
					for(d=0; d<this->nDaysPerWeek; d++)
						if(this->daysOfTheWeek[d]==text)
							break;

					if(d>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint TeacherNotAvailableTimes day corrupt for teacher %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->teacher)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
		
					assert(d<this->nDaysPerWeek);
					xmlReadingLog+="    Day="+this->daysOfTheWeek[d]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Hour"){
					QString text=xmlReader.readElementText();
					for(h=0; h < this->nHoursPerDay; h++)
						if(this->hoursOfTheDay[h]==text)
							break;
					
					if(h>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint TeacherNotAvailableTimes hour corrupt for teacher %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->teacher)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(h>=0 && h < this->nHoursPerDay);
					xmlReadingLog+="    Hour="+this->hoursOfTheDay[h]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			i++;
			
			cn->days.append(d);
			cn->hours.append(h);

			if(d==-1 || h==-1){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Not_Available_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacher=text;
			xmlReadingLog+="    Read teacher name="+cn->teacher+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	assert(i==cn->days.count() && i==cn->hours.count());
	if(!(i==nNotAvailableSlots)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Not_Available_Times").arg("Not_Available_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==nNotAvailableSlots);

	return cn;
}

TimeConstraint* Rules::readTeacherMaxDaysPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxDaysPerWeek");
	
	ConstraintTeacherMaxDaysPerWeek* cn=new ConstraintTeacherMaxDaysPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek<=0 || cn->maxDaysPerWeek>this->nDaysPerWeek){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Max_Days_Per_Week"));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeacherMaxDaysPerWeek max days corrupt for teacher %1, max days %2 <= 0 or >nDaysPerWeek, ignoring constraint")
					.arg(cn->teacherName)
					.arg(text));*/
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMaxDaysPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxDaysPerWeek");
	
	ConstraintTeachersMaxDaysPerWeek* cn=new ConstraintTeachersMaxDaysPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek<=0 || cn->maxDaysPerWeek>this->nDaysPerWeek){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Max_Days_Per_Week"));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeachersMaxDaysPerWeek max days corrupt, max days %1 <= 0 or >nDaysPerWeek, ignoring constraint")
					.arg(text));*/
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMinDaysPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMinDaysPerWeek");
	
	ConstraintTeacherMinDaysPerWeek* cn=new ConstraintTeacherMinDaysPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Minimum_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->minDaysPerWeek=text.toInt();
			if(cn->minDaysPerWeek<=0 || cn->minDaysPerWeek>this->nDaysPerWeek){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Minimum_Days_Per_Week"));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeacherMinDaysPerWeek min days corrupt for teacher %1, min days %2 <= 0 or >nDaysPerWeek, ignoring constraint")
					.arg(cn->teacherName)
					.arg(text));*/
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(cn->minDaysPerWeek>0 && cn->minDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Min. days per week="+CustomFETString::number(cn->minDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMinDaysPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMinDaysPerWeek");
	
	ConstraintTeachersMinDaysPerWeek* cn=new ConstraintTeachersMinDaysPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Minimum_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->minDaysPerWeek=text.toInt();
			if(cn->minDaysPerWeek<=0 || cn->minDaysPerWeek>this->nDaysPerWeek){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Minimum_Days_Per_Week"));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeachersMinDaysPerWeek min days corrupt, min days %1 <= 0 or >nDaysPerWeek, ignoring constraint")
					.arg(text));*/
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(cn->minDaysPerWeek>0 && cn->minDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Min. days per week="+CustomFETString::number(cn->minDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherIntervalMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherIntervalMaxDaysPerWeek");
	ConstraintTeacherIntervalMaxDaysPerWeek* cn=new ConstraintTeacherIntervalMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	cn->startHour=this->nHoursPerDay;
	cn->endHour=this->nHoursPerDay;
	int h1, h2;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint TeacherIntervalMaxDaysPerWeek max days corrupt for teacher %1, max days %2 >nDaysPerWeek, constraint added, please correct constraint")
					.arg(cn->teacherName)
					.arg(text));
				/*delete cn;
				cn=NULL;
				goto corruptConstraintTime;*/
			}
			//assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else if(xmlReader.name()=="Interval_Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint Teacher interval max days per week start hour corrupt for teacher %1, hour %2 is inexistent ... ignoring constraint")
					.arg(cn->teacherName)
					.arg(text));*/
				delete cn;
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Interval start hour="+this->hoursOfTheDay[h1]+"\n";
			cn->startHour=h1;
		}
		else if(xmlReader.name()=="Interval_End_Hour"){
			QString text=xmlReader.readElementText();
			if(text==""){
				xmlReadingLog+="    Interval end hour void, meaning end of day\n";
				cn->endHour=this->nHoursPerDay;
			}
			else{
				for(h2=0; h2 < this->nHoursPerDay; h2++)
					if(this->hoursOfTheDay[h2]==text)
						break;
				if(h2>=this->nHoursPerDay){
					xmlReader.raiseError(tr("Hour %1 is inexistent (it is also not void, to specify end of the day)").arg(text));
					/*RulesReconcilableMessage::information(parent, tr("FET information"),
						tr("Constraint Teacher interval max days per week end hour corrupt for teacher %1, hour %2 is inexistent (it is also not void, to specify end of the day) ... ignoring constraint")
						.arg(cn->teacherName)
						.arg(text));*/
					delete cn;
					//cn=NULL;
					//goto corruptConstraintTime;
					return NULL;
				}
				assert(h2>=0 && h2 < this->nHoursPerDay);
				xmlReadingLog+="    Interval end hour="+this->hoursOfTheDay[h2]+"\n";
				cn->endHour=h2;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersIntervalMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersIntervalMaxDaysPerWeek");
	ConstraintTeachersIntervalMaxDaysPerWeek* cn=new ConstraintTeachersIntervalMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	cn->startHour=this->nHoursPerDay;
	cn->endHour=this->nHoursPerDay;
	int h1, h2;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		/*else if(xmlReader.name()=="Teacher_Name"){
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}*/
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint TeachersIntervalMaxDaysPerWeek max days corrupt, max days %1 >nDaysPerWeek, constraint added, please correct constraint")
					//.arg(cn->teacherName)
					.arg(text));
				/*delete cn;
				cn=NULL;
				goto corruptConstraintTime;*/
			}
			//assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else if(xmlReader.name()=="Interval_Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint Teachers interval max days per week start hour corrupt because hour %1 is inexistent ... ignoring constraint")
					//.arg(cn->teacherName)
					.arg(text));*/
				delete cn;
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Interval start hour="+this->hoursOfTheDay[h1]+"\n";
			cn->startHour=h1;
		}
		else if(xmlReader.name()=="Interval_End_Hour"){
			QString text=xmlReader.readElementText();
			if(text==""){
				xmlReadingLog+="    Interval end hour void, meaning end of day\n";
				cn->endHour=this->nHoursPerDay;
			}
			else{
				for(h2=0; h2 < this->nHoursPerDay; h2++)
					if(this->hoursOfTheDay[h2]==text)
						break;
				if(h2>=this->nHoursPerDay){
					xmlReader.raiseError(tr("Hour %1 is inexistent (it is also not void, to specify end of the day)").arg(text));
					/*RulesReconcilableMessage::information(parent, tr("FET information"),
						tr("Constraint Teachers interval max days per week end hour corrupt because hour %1 is inexistent (it is also not void, to specify end of the day) ... ignoring constraint")
						//.arg(cn->teacherName)
						.arg(text));*/
					delete cn;
					//cn=NULL;
					//goto corruptConstraintTime;
					return NULL;
				}
				assert(h2>=0 && h2 < this->nHoursPerDay);
				xmlReadingLog+="    Interval end hour="+this->hoursOfTheDay[h2]+"\n";
				cn->endHour=h2;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxDaysPerWeek");
	ConstraintStudentsSetMaxDaysPerWeek* cn=new ConstraintStudentsSetMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students set name="+cn->students+"\n";
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsSetMaxDaysPerWeek max days corrupt for students set %1, max days %2 >nDaysPerWeek, constraint added, please correct constraint")
					.arg(cn->students)
					.arg(text));
			}
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxDaysPerWeek");
	ConstraintStudentsMaxDaysPerWeek* cn=new ConstraintStudentsMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsMaxDaysPerWeek max days corrupt, max days %1 >nDaysPerWeek, constraint added, please correct constraint")
					.arg(text));
			}
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetIntervalMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetIntervalMaxDaysPerWeek");
	ConstraintStudentsSetIntervalMaxDaysPerWeek* cn=new ConstraintStudentsSetIntervalMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	cn->startHour=this->nHoursPerDay;
	cn->endHour=this->nHoursPerDay;
	int h1, h2;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students set name="+cn->students+"\n";
		}
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsSetIntervalMaxDaysPerWeek max days corrupt for students set %1, max days %2 >nDaysPerWeek, constraint added, please correct constraint")
					.arg(cn->students)
					.arg(text));
				/*delete cn;
				cn=NULL;
				goto corruptConstraintTime;*/
			}
			//assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else if(xmlReader.name()=="Interval_Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint Students set interval max days per week start hour corrupt for students %1, hour %2 is inexistent ... ignoring constraint")
					.arg(cn->students)
					.arg(text));*/
				delete cn;
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Interval start hour="+this->hoursOfTheDay[h1]+"\n";
			cn->startHour=h1;
		}
		else if(xmlReader.name()=="Interval_End_Hour"){
			QString text=xmlReader.readElementText();
			if(text==""){
				xmlReadingLog+="    Interval end hour void, meaning end of day\n";
				cn->endHour=this->nHoursPerDay;
			}
			else{
				for(h2=0; h2 < this->nHoursPerDay; h2++)
					if(this->hoursOfTheDay[h2]==text)
						break;
				if(h2>=this->nHoursPerDay){
					xmlReader.raiseError(tr("Hour %1 is inexistent (it is also not void, to specify end of the day)").arg(text));
					/*RulesReconcilableMessage::information(parent, tr("FET information"),
						tr("Constraint Students set interval max days per week end hour corrupt for students %1, hour %2 is inexistent (it is also not void, to specify end of the day) ... ignoring constraint")
						.arg(cn->students)
						.arg(text));*/
					delete cn;
					//cn=NULL;
					//goto corruptConstraintTime;
					return NULL;
				}
				assert(h2>=0 && h2 < this->nHoursPerDay);
				xmlReadingLog+="    Interval end hour="+this->hoursOfTheDay[h2]+"\n";
				cn->endHour=h2;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsIntervalMaxDaysPerWeek(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsIntervalMaxDaysPerWeek");
	ConstraintStudentsIntervalMaxDaysPerWeek* cn=new ConstraintStudentsIntervalMaxDaysPerWeek();
	cn->maxDaysPerWeek=this->nDaysPerWeek;
	cn->startHour=this->nHoursPerDay;
	cn->endHour=this->nHoursPerDay;
	int h1, h2;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating 100% weight percentage\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		/*else if(xmlReader.name()=="Students"){
			cn->students=text;
			xmlReadingLog+="    Read students set name="+cn->students+"\n";
		}*/
		else if(xmlReader.name()=="Max_Days_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxDaysPerWeek=text.toInt();
			if(cn->maxDaysPerWeek>this->nDaysPerWeek){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsIntervalMaxDaysPerWeek max days corrupt: max days %1 >nDaysPerWeek, constraint added, please correct constraint")
					.arg(text));
				/*delete cn;
				cn=NULL;
				goto corruptConstraintTime;*/
			}
			//assert(cn->maxDaysPerWeek>0 && cn->maxDaysPerWeek <= this->nDaysPerWeek);
			xmlReadingLog+="    Max. days per week="+CustomFETString::number(cn->maxDaysPerWeek)+"\n";
		}
		else if(xmlReader.name()=="Interval_Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint Students interval max days per week start hour corrupt: hour %1 is inexistent ... ignoring constraint")
					//.arg(cn->students)
					.arg(text));*/
				delete cn;
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Interval start hour="+this->hoursOfTheDay[h1]+"\n";
			cn->startHour=h1;
		}
		else if(xmlReader.name()=="Interval_End_Hour"){
			QString text=xmlReader.readElementText();
			if(text==""){
				xmlReadingLog+="    Interval end hour void, meaning end of day\n";
				cn->endHour=this->nHoursPerDay;
			}
			else{
				for(h2=0; h2 < this->nHoursPerDay; h2++)
					if(this->hoursOfTheDay[h2]==text)
						break;
				if(h2>=this->nHoursPerDay){
					xmlReader.raiseError(tr("Hour %1 is inexistent (it is also not void, to specify end of the day)").arg(text));
					/*RulesReconcilableMessage::information(parent, tr("FET information"),
						tr("Constraint Students interval max days per week end hour corrupt: hour %1 is inexistent (it is also not void, to specify end of the day) ... ignoring constraint")
						//.arg(cn->students)
						.arg(text));*/
					delete cn;
					//cn=NULL;
					//goto corruptConstraintTime;
					return NULL;
				}
				assert(h2>=0 && h2 < this->nHoursPerDay);
				xmlReadingLog+="    Interval end hour="+this->hoursOfTheDay[h2]+"\n";
				cn->endHour=h2;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetNotAvailable(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetNotAvailable");

	//ConstraintStudentsSetNotAvailableTimes* cn=new ConstraintStudentsSetNotAvailableTimes();
	QList<int> days;
	QList<int> hours;
	QString students;
	double weightPercentage=100;
	int d=-1, h1=-1, h2=-1;
	bool active=true;
	QString comments=QString("");
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			weightPercentage=customFETStrToDouble(text);
			if(weightPercentage<0){
				xmlReader.raiseError(tr("Weight percentage incorrect"));
				return NULL;
			}
			assert(weightPercentage>=0);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			comments=text;
		}
		else if(xmlReader.name()=="Day"){
			QString text=xmlReader.readElementText();
			for(d=0; d<this->nDaysPerWeek; d++)
				if(this->daysOfTheWeek[d]==text)
					break;
			if(d>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsSetNotAvailable day corrupt for students %1, day %2 is inexistent ... ignoring constraint")
					.arg(students)
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(d<this->nDaysPerWeek);
			xmlReadingLog+="    Crt. day="+this->daysOfTheWeek[d]+"\n";
		}
		else if(xmlReader.name()=="Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1==this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is the last hour - impossible").arg(text));
				return NULL;
			}
			else if(h1>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsSetNotAvailable start hour corrupt for students set %1, hour %2 is inexistent ... ignoring constraint")
					.arg(students)
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Start hour="+this->hoursOfTheDay[h1]+"\n";
		}
		else if(xmlReader.name()=="End_Hour"){
			QString text=xmlReader.readElementText();
			for(h2=0; h2 < this->nHoursPerDay; h2++)
				if(this->hoursOfTheDay[h2]==text)
					break;
			if(h2==0){
				xmlReader.raiseError(tr("Hour %1 is the first hour - impossible").arg(text));
				return NULL;
			}
			else if(h2>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				return NULL;
			}
			/*if(h2<=0 || h2>this->nHoursPerDay){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint StudentsSetNotAvailable end hour corrupt for students %1, hour %2 is inexistent ... ignoring constraint")
					.arg(students)
					.arg(text));
				//goto corruptConstraintTime;
				return NULL;
			}*/
			assert(h2>0 && h2 <= this->nHoursPerDay);
			xmlReadingLog+="    End hour="+this->hoursOfTheDay[h2]+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			students=text;
			xmlReadingLog+="    Read students name="+students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	assert(weightPercentage>=0);
	if(d<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Day"));
		return NULL;
	}
	else if(h1<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Start_Hour"));
		return NULL;
	}
	else if(h2<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("End_Hour"));
		return NULL;
	}
	assert(d>=0 && h1>=0 && h2>=0);
	
	ConstraintStudentsSetNotAvailableTimes* cn = NULL;
	
	bool found=false;
	foreach(TimeConstraint* c, this->timeConstraintsList)
		if(c->type==CONSTRAINT_STUDENTS_SET_NOT_AVAILABLE_TIMES){
			ConstraintStudentsSetNotAvailableTimes* ssna=(ConstraintStudentsSetNotAvailableTimes*) c;
			if(ssna->students==students){
				found=true;
				
				for(int hh=h1; hh<h2; hh++){
					int k;
					for(k=0; k<ssna->days.count(); k++)
						if(ssna->days.at(k)==d && ssna->hours.at(k)==hh)
							break;
					if(k==ssna->days.count()){
						ssna->days.append(d);
						ssna->hours.append(hh);
					}
				}
				
				assert(ssna->days.count()==ssna->hours.count());
			}
		}
	if(!found){
		days.clear();
		hours.clear();
		for(int hh=h1; hh<h2; hh++){
			days.append(d);
			hours.append(hh);
		}
	
		cn=new ConstraintStudentsSetNotAvailableTimes(weightPercentage, students, days, hours);
		cn->active=active;
		cn->comments=comments;

		//crt_constraint=cn;
		return cn;
	}
	else
		return NULL;
}

TimeConstraint* Rules::readStudentsSetNotAvailableTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetNotAvailableTimes");
	
	ConstraintStudentsSetNotAvailableTimes* cn=new ConstraintStudentsSetNotAvailableTimes();
	int nNotAvailableSlots=0;
	int i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}

		else if(xmlReader.name()=="Number_of_Not_Available_Times"){
			QString text=xmlReader.readElementText();
			nNotAvailableSlots=text.toInt();
			xmlReadingLog+="    Read number of not available times="+CustomFETString::number(nNotAvailableSlots)+"\n";
		}

		else if(xmlReader.name()=="Not_Available_Time"){
			xmlReadingLog+="    Read: not available time\n";
			
			int d=-1;
			int h=-1;

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Day"){
					QString text=xmlReader.readElementText();
					for(d=0; d<this->nDaysPerWeek; d++)
						if(this->daysOfTheWeek[d]==text)
							break;

					if(d>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint StudentsSetNotAvailableTimes day corrupt for students %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->students)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
		
					assert(d<this->nDaysPerWeek);
					xmlReadingLog+="    Day="+this->daysOfTheWeek[d]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Hour"){
					QString text=xmlReader.readElementText();
					for(h=0; h < this->nHoursPerDay; h++)
						if(this->hoursOfTheDay[h]==text)
							break;
					
					if(h>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint StudentsSetNotAvailableTimes hour corrupt for students %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->students)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(h>=0 && h < this->nHoursPerDay);
					xmlReadingLog+="    Hour="+this->hoursOfTheDay[h]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			i++;
			
			cn->days.append(d);
			cn->hours.append(h);

			if(d==-1 || h==-1){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Not_Available_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	assert(i==cn->days.count() && i==cn->hours.count());
	if(!(i==nNotAvailableSlots)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Not_Available_Times").arg("Not_Available_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==nNotAvailableSlots);

	return cn;
}

TimeConstraint* Rules::readMinNDaysBetweenActivities(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintMinNDaysBetweenActivities");
	
	ConstraintMinDaysBetweenActivities* cn=new ConstraintMinDaysBetweenActivities();
	bool foundCISD=false;
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating weightPercentage=95%\n";
			cn->weightPercentage=95;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weightPercentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Consecutive_If_Same_Day" || xmlReader.name()=="Adjacent_If_Broken"){
			QString text=xmlReader.readElementText();
			if(text=="yes" || text=="true" || text=="1"){
				cn->consecutiveIfSameDay=true;
				foundCISD=true;
				xmlReadingLog+="    Current constraint has consecutive if same day=true\n";
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint min days between activities with tag consecutive if same day"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="no" || text=="false" || text=="0");
				cn->consecutiveIfSameDay=false;
				foundCISD=true;
				xmlReadingLog+="    Current constraint has consecutive if same day=false\n";
			}
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=95;
				cn->consecutiveIfSameDay=true;
				foundCISD=true;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
				cn->consecutiveIfSameDay=false;
				foundCISD=true;
			}
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else if(xmlReader.name()=="MinDays"){
			QString text=xmlReader.readElementText();
			cn->minDays=text.toInt();
			xmlReadingLog+="    Read MinDays="+CustomFETString::number(cn->minDays)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!foundCISD){
		xmlReadingLog+="    Could not find consecutive if same day information - making it true\n";
		cn->consecutiveIfSameDay=true;
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
/*
#if 0
	if(0 && reportIncorrectMinDays && cn->n_activities > this->nDaysPerWeek){
		QString s=tr("You have a constraint min days between activities with more activities than the number of days per week.");
		s+=" ";
		s+=tr("Constraint is:");
		s+="\n";
		s+=crt_constraint->getDescription(*this);
		s+="\n";
		s+=tr("This is a very bad practice from the way the algorithm of generation works (it slows down the generation and makes it harder to find a solution).");
		s+="\n\n";
		s+=tr("To improve your file, you are advised to remove the corresponding activities and constraint and add activities again, respecting the following rules:");
		s+="\n\n";
		s+=tr("1. If you add 'force consecutive if same day', then couple extra activities in pairs to obtain a number of activities equal to the number of days per week"
			". Example: 7 activities with duration 1 in a 5 days week, then transform into 5 activities with durations: 2,2,1,1,1 and add a single container activity with these 5 components"
			" (possibly raising the weight of added constraint min days between activities up to 100%)");
		s+="\n\n";

		s+=tr("2. If you don't add 'force consecutive if same day', then add a larger activity splitted into a number of"
			" activities equal with the number of days per week and the remaining components into other larger splitted activity."
			" For example, suppose you need to add 7 activities with duration 1 in a 5 days week. Add 2 larger container activities,"
			" first one splitted into 5 activities with duration 1 and second one splitted into 2 activities with duration 1"
			" (possibly raising the weight of added constraints min days between activities for each of the 2 containers up to 100%)");
		
		int t=QMessageBox::warning(parent, tr("FET warning"), s,
			tr("Skip rest"), tr("See next"), QString(),
			1, 0 );
										
		if(t==0)
			reportIncorrectMinDays=false;
	}
#endif
*/
}

TimeConstraint* Rules::readMinDaysBetweenActivities(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintMinDaysBetweenActivities");
	
	ConstraintMinDaysBetweenActivities* cn=new ConstraintMinDaysBetweenActivities();
	bool foundCISD=false;
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - generating weightPercentage=95%\n";
			cn->weightPercentage=95;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weightPercentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Consecutive_If_Same_Day" || xmlReader.name()=="Adjacent_If_Broken"){
			QString text=xmlReader.readElementText();
			if(text=="yes" || text=="true" || text=="1"){
				cn->consecutiveIfSameDay=true;
				foundCISD=true;
				xmlReadingLog+="    Current constraint has consecutive if same day=true\n";
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint min days between activities with tag consecutive if same day"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="no" || text=="false" || text=="0");
				cn->consecutiveIfSameDay=false;
				foundCISD=true;
				xmlReadingLog+="    Current constraint has consecutive if same day=false\n";
			}
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=95;
				cn->consecutiveIfSameDay=true;
				foundCISD=true;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
				cn->consecutiveIfSameDay=false;
				foundCISD=true;
			}
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else if(xmlReader.name()=="MinDays"){
			QString text=xmlReader.readElementText();
			cn->minDays=text.toInt();
			xmlReadingLog+="    Read MinDays="+CustomFETString::number(cn->minDays)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!foundCISD){
		xmlReadingLog+="    Could not find consecutive if same day information - making it true\n";
		cn->consecutiveIfSameDay=true;
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
/*
#if 0
	if(0 && reportIncorrectMinDays && cn->n_activities > this->nDaysPerWeek){
		QString s=tr("You have a constraint min days between activities with more activities than the number of days per week.");
		s+=" ";
		s+=tr("Constraint is:");
		s+="\n";
		s+=crt_constraint->getDescription(*this);
		s+="\n";
		s+=tr("This is a very bad practice from the way the algorithm of generation works (it slows down the generation and makes it harder to find a solution).");
		s+="\n\n";
		s+=tr("To improve your file, you are advised to remove the corresponding activities and constraint and add activities again, respecting the following rules:");
		s+="\n\n";
		s+=tr("1. If you add 'force consecutive if same day', then couple extra activities in pairs to obtain a number of activities equal to the number of days per week"
			". Example: 7 activities with duration 1 in a 5 days week, then transform into 5 activities with durations: 2,2,1,1,1 and add a single container activity with these 5 components"
			" (possibly raising the weight of added constraint min days between activities up to 100%)");
		s+="\n\n";

		s+=tr("2. If you don't add 'force consecutive if same day', then add a larger activity splitted into a number of"
			" activities equal with the number of days per week and the remaining components into other larger splitted activity."
			" For example, suppose you need to add 7 activities with duration 1 in a 5 days week. Add 2 larger container activities,"
			" first one splitted into 5 activities with duration 1 and second one splitted into 2 activities with duration 1"
			" (possibly raising the weight of added constraints min days between activities for each of the 2 containers up to 100%)");
		
		int t=QMessageBox::warning(parent, tr("FET warning"), s,
			tr("Skip rest"), tr("See next"), QString(),
			1, 0 );
										
		if(t==0)
			reportIncorrectMinDays=false;
	}
#endif
*/
}

TimeConstraint* Rules::readMaxDaysBetweenActivities(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintMaxDaysBetweenActivities");
	
	ConstraintMaxDaysBetweenActivities* cn=new ConstraintMaxDaysBetweenActivities();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weightPercentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			//cn->activitiesId[n_act]=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else if(xmlReader.name()=="MaxDays"){
			QString text=xmlReader.readElementText();
			cn->maxDays=text.toInt();
			xmlReadingLog+="    Read MaxDays="+CustomFETString::number(cn->maxDays)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readMinGapsBetweenActivities(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintMinGapsBetweenActivities");
	ConstraintMinGapsBetweenActivities* cn=new ConstraintMinGapsBetweenActivities();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weightPercentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else if(xmlReader.name()=="MinGaps"){
			QString text=xmlReader.readElementText();
			cn->minGaps=text.toInt();
			xmlReadingLog+="    Read MinGaps="+CustomFETString::number(cn->minGaps)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readActivitiesNotOverlapping(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesNotOverlapping");
	ConstraintActivitiesNotOverlapping* cn=new ConstraintActivitiesNotOverlapping();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readActivitiesSameStartingTime(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesSameStartingTime");
	ConstraintActivitiesSameStartingTime* cn=new ConstraintActivitiesSameStartingTime();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readActivitiesSameStartingHour(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesSameStartingHour");
	ConstraintActivitiesSameStartingHour* cn=new ConstraintActivitiesSameStartingHour();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readActivitiesSameStartingDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesSameStartingDay");
	ConstraintActivitiesSameStartingDay* cn=new ConstraintActivitiesSameStartingDay();
	int n_act=0;
	cn->activitiesId.clear();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			cn->n_activities=text.toInt();
			xmlReadingLog+="    Read n_activities="+CustomFETString::number(cn->n_activities)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			//cn->activitiesId[n_act]=text.toInt();
			cn->activitiesId.append(text.toInt());
			assert(n_act==cn->activitiesId.count()-1);
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesId[n_act])+"\n";
			n_act++;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(n_act==cn->n_activities)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(n_act==cn->n_activities);
	return cn;
}

TimeConstraint* Rules::readTeachersMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxHoursDaily");
	ConstraintTeachersMaxHoursDaily* cn=new ConstraintTeachersMaxHoursDaily();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxHoursDaily");
	ConstraintTeacherMaxHoursDaily* cn=new ConstraintTeacherMaxHoursDaily();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxHoursContinuously");
	ConstraintTeachersMaxHoursContinuously* cn=new ConstraintTeachersMaxHoursContinuously();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxHoursContinuously");
	ConstraintTeacherMaxHoursContinuously* cn=new ConstraintTeacherMaxHoursContinuously();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherActivityTagMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherActivityTagMaxHoursContinuously");
	ConstraintTeacherActivityTagMaxHoursContinuously* cn=new ConstraintTeacherActivityTagMaxHoursContinuously();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersActivityTagMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersActivityTagMaxHoursContinuously");
	ConstraintTeachersActivityTagMaxHoursContinuously* cn=new ConstraintTeachersActivityTagMaxHoursContinuously();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherActivityTagMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherActivityTagMaxHoursDaily");
	ConstraintTeacherActivityTagMaxHoursDaily* cn=new ConstraintTeacherActivityTagMaxHoursDaily();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersActivityTagMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersActivityTagMaxHoursDaily");
	ConstraintTeachersActivityTagMaxHoursDaily* cn=new ConstraintTeachersActivityTagMaxHoursDaily();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMinHoursDaily(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMinHoursDaily");
	ConstraintTeachersMinHoursDaily* cn=new ConstraintTeachersMinHoursDaily();
	cn->allowEmptyDays=true;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Minimum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->minHoursDaily=text.toInt();
			xmlReadingLog+="    Read minHoursDaily="+CustomFETString::number(cn->minHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Allow_Empty_Days"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Read allow empty days=true\n";
				cn->allowEmptyDays=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint teachers min hours daily with tag allow empty days"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Read allow empty days=false\n";
				cn->allowEmptyDays=false;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMinHoursDaily(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMinHoursDaily");
	ConstraintTeacherMinHoursDaily* cn=new ConstraintTeacherMinHoursDaily();
	cn->allowEmptyDays=true;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Minimum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->minHoursDaily=text.toInt();
			xmlReadingLog+="    Read minHoursDaily="+CustomFETString::number(cn->minHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Allow_Empty_Days"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Read allow empty days=true\n";
				cn->allowEmptyDays=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint teacher min hours daily with tag allow empty days"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Read allow empty days=false\n";
				cn->allowEmptyDays=false;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxHoursDaily");
	ConstraintStudentsMaxHoursDaily* cn=new ConstraintStudentsMaxHoursDaily();
	cn->maxHoursDaily=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			if(cn->maxHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxHoursDaily");
	ConstraintStudentsSetMaxHoursDaily* cn=new ConstraintStudentsSetMaxHoursDaily();
	cn->maxHoursDaily=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			if(cn->maxHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxHoursContinuously");
	ConstraintStudentsMaxHoursContinuously* cn=new ConstraintStudentsMaxHoursContinuously();
	cn->maxHoursContinuously=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			if(cn->maxHoursContinuously<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Continuously"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursContinuously<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Continuously"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursContinuously>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxHoursContinuously");
	ConstraintStudentsSetMaxHoursContinuously* cn=new ConstraintStudentsSetMaxHoursContinuously();
	cn->maxHoursContinuously=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			if(cn->maxHoursContinuously<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Continuously"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursContinuously<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Continuously"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursContinuously>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetActivityTagMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetActivityTagMaxHoursContinuously");
	ConstraintStudentsSetActivityTagMaxHoursContinuously* cn=new ConstraintStudentsSetActivityTagMaxHoursContinuously();
	cn->maxHoursContinuously=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			if(cn->maxHoursContinuously<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Continuously"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursContinuously<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Continuously"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursContinuously>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsActivityTagMaxHoursContinuously(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsActivityTagMaxHoursContinuously");
	ConstraintStudentsActivityTagMaxHoursContinuously* cn=new ConstraintStudentsActivityTagMaxHoursContinuously();
	cn->maxHoursContinuously=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Continuously"){
			QString text=xmlReader.readElementText();
			cn->maxHoursContinuously=text.toInt();
			if(cn->maxHoursContinuously<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Continuously"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursContinuously="+CustomFETString::number(cn->maxHoursContinuously)+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursContinuously<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Continuously"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursContinuously>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetActivityTagMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetActivityTagMaxHoursDaily");
	ConstraintStudentsSetActivityTagMaxHoursDaily* cn=new ConstraintStudentsSetActivityTagMaxHoursDaily();
	cn->maxHoursDaily=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			if(cn->maxHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsActivityTagMaxHoursDaily(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsActivityTagMaxHoursDaily");
	ConstraintStudentsActivityTagMaxHoursDaily* cn=new ConstraintStudentsActivityTagMaxHoursDaily();
	cn->maxHoursDaily=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Maximum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->maxHoursDaily=text.toInt();
			if(cn->maxHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Maximum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read maxHoursDaily="+CustomFETString::number(cn->maxHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->maxHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Maximum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsMinHoursDaily(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMinHoursDaily");
	ConstraintStudentsMinHoursDaily* cn=new ConstraintStudentsMinHoursDaily();
	cn->minHoursDaily=-1;
	cn->allowEmptyDays=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Minimum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->minHoursDaily=text.toInt();
			if(cn->minHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Minimum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read minHoursDaily="+CustomFETString::number(cn->minHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Allow_Empty_Days"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Read allow empty days=true\n";
				cn->allowEmptyDays=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint students min hours daily with tag allow empty days"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Read allow empty days=false\n";
				cn->allowEmptyDays=false;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->minHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Minimum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->minHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetMinHoursDaily(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMinHoursDaily");
	ConstraintStudentsSetMinHoursDaily* cn=new ConstraintStudentsSetMinHoursDaily();
	cn->minHoursDaily=-1;
	cn->allowEmptyDays=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Minimum_Hours_Daily"){
			QString text=xmlReader.readElementText();
			cn->minHoursDaily=text.toInt();
			if(cn->minHoursDaily<0){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Minimum_Hours_Daily"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Read minHoursDaily="+CustomFETString::number(cn->minHoursDaily)+"\n";
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else if(xmlReader.name()=="Allow_Empty_Days"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Read allow empty days=true\n";
				cn->allowEmptyDays=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint students set min hours daily with tag allow empty days"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Read allow empty days=false\n";
				cn->allowEmptyDays=false;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(cn->minHoursDaily<0){
		xmlReader.raiseError(tr("%1 not found").arg("Minimum_Hours_Daily"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->minHoursDaily>=0);
	return cn;
}

TimeConstraint* Rules::readActivityPreferredTime(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog,
bool& reportUnspecifiedPermanentlyLockedTime, bool& reportUnspecifiedDayOrHourPreferredStartingTime){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredTime");

	ConstraintActivityPreferredStartingTime* cn=new ConstraintActivityPreferredStartingTime();
	cn->day = cn->hour = -1;
	cn->permanentlyLocked=false; //default not locked
	bool foundLocked=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Permanently_Locked"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Permanently locked\n";
				cn->permanentlyLocked=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint activity preferred starting time with tag permanently locked"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Not permanently locked\n";
				cn->permanentlyLocked=false;
			}
			foundLocked=true;
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Day"){
			QString text=xmlReader.readElementText();
			for(cn->day=0; cn->day<this->nDaysPerWeek; cn->day++)
				if(this->daysOfTheWeek[cn->day]==text)
					break;
			if(cn->day>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint ActivityPreferredTime day corrupt for activity with id %1, day %2 is inexistent ... ignoring constraint")
					.arg(cn->activityId)
					.arg(text));*/
				delete cn;
				cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(cn->day<this->nDaysPerWeek);
			xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->day]+"\n";
		}
		else if(xmlReader.name()=="Preferred_Hour"){
			QString text=xmlReader.readElementText();
			for(cn->hour=0; cn->hour < this->nHoursPerDay; cn->hour++)
				if(this->hoursOfTheDay[cn->hour]==text)
					break;
			if(cn->hour>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint ActivityPreferredTime hour corrupt for activity with id %1, hour %2 is inexistent ... ignoring constraint")
					.arg(cn->activityId)
					.arg(text));*/
				delete cn;
				cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(cn->hour>=0 && cn->hour < this->nHoursPerDay);
			xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->hour]+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	//crt_constraint=cn;

	if(cn->hour>=0 && cn->day>=0 && !foundLocked && reportUnspecifiedPermanentlyLockedTime){
		int t=RulesReconcilableMessage::information(parent, tr("FET information"),
			tr("Found constraint activity preferred starting time, with unspecified tag"
			" 'permanently locked' - this tag will be set to 'false' by default. You can always modify it"
			" by editing the constraint in the 'Data' menu")+"\n\n"
			+tr("Explanation: starting with version 5.8.0 (January 2009), the constraint"
			" activity preferred starting time has"
			" a new tag, 'permanently locked' (true or false)."
			" It is recommended to make the tag 'permanently locked' true for the constraints you"
			" need to be not modifiable from the 'Timetable' menu"
			" and leave this tag false for the constraints you need to be modifiable from the 'Timetable' menu"
			" (the 'permanently locked' tag can be modified by editing the constraint from the 'Data' menu)."
			" This way, when viewing the timetable"
			" and locking/unlocking some activities, you will not unlock the constraints which"
			" need to be locked all the time."
			),
			tr("Skip rest"), tr("See next"), QString(), 1, 0 );
		if(t==0)
			reportUnspecifiedPermanentlyLockedTime=false;
	}
	
	if(cn->hour==-1 || cn->day==-1){
		if(reportUnspecifiedDayOrHourPreferredStartingTime){
			int t=RulesReconcilableMessage::information(parent, tr("FET information"),
				tr("Found constraint activity preferred starting time, with unspecified day or hour."
				" This constraint will be transformed into constraint activity preferred starting times (a set of times, not only one)."
				" This change is done in FET versions 5.8.1 and higher."
				),
				tr("Skip rest"), tr("See next"), QString(), 1, 0 );
			if(t==0)
				reportUnspecifiedDayOrHourPreferredStartingTime=false;
		}
			
		ConstraintActivityPreferredStartingTimes* cgood=new ConstraintActivityPreferredStartingTimes();
		if(cn->day==-1){
			cgood->activityId=cn->activityId;
			cgood->weightPercentage=cn->weightPercentage;
			cgood->nPreferredStartingTimes_L=this->nDaysPerWeek;
			for(int i=0; i<cgood->nPreferredStartingTimes_L; i++){
				/*cgood->days[i]=i;
				cgood->hours[i]=cn->hour;*/
				cgood->days_L.append(i);
				cgood->hours_L.append(cn->hour);
			}
		}
		else{
			assert(cn->hour==-1);
			cgood->activityId=cn->activityId;
			cgood->weightPercentage=cn->weightPercentage;
			cgood->nPreferredStartingTimes_L=this->nHoursPerDay;
			for(int i=0; i<cgood->nPreferredStartingTimes_L; i++){
				/*cgood->days[i]=cn->day;
				cgood->hours[i]=i;*/
				cgood->days_L.append(cn->day);
				cgood->hours_L.append(i);
			}
		}
		
		delete cn;
		return cgood;
	}
	
	return cn;
}

TimeConstraint* Rules::readActivityPreferredStartingTime(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog,
bool& reportUnspecifiedPermanentlyLockedTime, bool& reportUnspecifiedDayOrHourPreferredStartingTime){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredStartingTime");
	ConstraintActivityPreferredStartingTime* cn=new ConstraintActivityPreferredStartingTime();
	cn->day = cn->hour = -1;
	cn->permanentlyLocked=false; //default false
	bool foundLocked=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Permanently_Locked"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Permanently locked\n";
				cn->permanentlyLocked=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint activity preferred starting time with tag permanently locked"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Not permanently locked\n";
				cn->permanentlyLocked=false;
			}
			foundLocked=true;
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Day"){
			QString text=xmlReader.readElementText();
			for(cn->day=0; cn->day<this->nDaysPerWeek; cn->day++)
				if(this->daysOfTheWeek[cn->day]==text)
					break;
			if(cn->day>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint ActivityPreferredStartingTime day corrupt for activity with id %1, day %2 is inexistent ... ignoring constraint")
					.arg(cn->activityId)
					.arg(text));*/
				delete cn;
				cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(cn->day<this->nDaysPerWeek);
			xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->day]+"\n";
		}
		else if(xmlReader.name()=="Preferred_Hour"){
			QString text=xmlReader.readElementText();
			for(cn->hour=0; cn->hour < this->nHoursPerDay; cn->hour++)
				if(this->hoursOfTheDay[cn->hour]==text)
					break;
			if(cn->hour>=this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint ActivityPreferredStartingTime hour corrupt for activity with id %1, hour %2 is inexistent ... ignoring constraint")
					.arg(cn->activityId)
					.arg(text));*/
				delete cn;
				cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(cn->hour>=0 && cn->hour < this->nHoursPerDay);
			xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->hour]+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	//crt_constraint=cn;

	if(cn->hour>=0 && cn->day>=0 && !foundLocked && reportUnspecifiedPermanentlyLockedTime){
		int t=RulesReconcilableMessage::information(parent, tr("FET information"),
			tr("Found constraint activity preferred starting time, with unspecified tag"
			" 'permanently locked' - this tag will be set to 'false' by default. You can always modify it"
			" by editing the constraint in the 'Data' menu")+"\n\n"
			+tr("Explanation: starting with version 5.8.0 (January 2009), the constraint"
			" activity preferred starting time has"
			" a new tag, 'permanently locked' (true or false)."
			" It is recommended to make the tag 'permanently locked' true for the constraints you"
			" need to be not modifiable from the 'Timetable' menu"
			" and leave this tag false for the constraints you need to be modifiable from the 'Timetable' menu"
			" (the 'permanently locked' tag can be modified by editing the constraint from the 'Data' menu)."
			" This way, when viewing the timetable"
			" and locking/unlocking some activities, you will not unlock the constraints which"
			" need to be locked all the time."
			),
			tr("Skip rest"), tr("See next"), QString(), 1, 0 );
		if(t==0)
			reportUnspecifiedPermanentlyLockedTime=false;
	}

	if(cn->hour==-1 || cn->day==-1){
		if(reportUnspecifiedDayOrHourPreferredStartingTime){
			int t=RulesReconcilableMessage::information(parent, tr("FET information"),
				tr("Found constraint activity preferred starting time, with unspecified day or hour."
				" This constraint will be transformed into constraint activity preferred starting times (a set of times, not only one)."
				" This change is done in FET versions 5.8.1 and higher."
				),
				tr("Skip rest"), tr("See next"), QString(), 1, 0 );
			if(t==0)
				reportUnspecifiedDayOrHourPreferredStartingTime=false;
		}
			
		ConstraintActivityPreferredStartingTimes* cgood=new ConstraintActivityPreferredStartingTimes();
		if(cn->day==-1){
			cgood->activityId=cn->activityId;
			cgood->weightPercentage=cn->weightPercentage;
			cgood->nPreferredStartingTimes_L=this->nDaysPerWeek;
			for(int i=0; i<cgood->nPreferredStartingTimes_L; i++){
				/*cgood->days[i]=i;
				cgood->hours[i]=cn->hour;*/
				cgood->days_L.append(i);
				cgood->hours_L.append(cn->hour);
			}
		}
		else{
			assert(cn->hour==-1);
			cgood->activityId=cn->activityId;
			cgood->weightPercentage=cn->weightPercentage;
			cgood->nPreferredStartingTimes_L=this->nHoursPerDay;
			for(int i=0; i<cgood->nPreferredStartingTimes_L; i++){
				/*cgood->days[i]=cn->day;
				cgood->hours[i]=i;*/
				cgood->days_L.append(cn->day);
				cgood->hours_L.append(i);
			}
		}
		
		delete cn;
		return cgood;
	}
	
	return cn;
}

TimeConstraint* Rules::readActivityEndsStudentsDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityEndsStudentsDay");
	ConstraintActivityEndsStudentsDay* cn=new ConstraintActivityEndsStudentsDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readActivitiesEndStudentsDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesEndStudentsDay");
	ConstraintActivitiesEndStudentsDay* cn=new ConstraintActivitiesEndStudentsDay();
	cn->teacherName="";
	cn->studentsName="";
	cn->subjectName="";
	cn->activityTagName="";
	
	//i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::read2ActivitiesConsecutive(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="Constraint2ActivitiesConsecutive");
	ConstraintTwoActivitiesConsecutive* cn=new ConstraintTwoActivitiesConsecutive();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::read2ActivitiesGrouped(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="Constraint2ActivitiesGrouped");
	ConstraintTwoActivitiesGrouped* cn=new ConstraintTwoActivitiesGrouped();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::read3ActivitiesGrouped(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="Constraint3ActivitiesGrouped");
	ConstraintThreeActivitiesGrouped* cn=new ConstraintThreeActivitiesGrouped();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else if(xmlReader.name()=="Third_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->thirdActivityId=text.toInt();
			xmlReadingLog+="    Read third activity id="+CustomFETString::number(cn->thirdActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::read2ActivitiesOrdered(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="Constraint2ActivitiesOrdered");
	ConstraintTwoActivitiesOrdered* cn=new ConstraintTwoActivitiesOrdered();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTwoActivitiesConsecutive(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTwoActivitiesConsecutive");
	ConstraintTwoActivitiesConsecutive* cn=new ConstraintTwoActivitiesConsecutive();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTwoActivitiesGrouped(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTwoActivitiesGrouped");
	ConstraintTwoActivitiesGrouped* cn=new ConstraintTwoActivitiesGrouped();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readThreeActivitiesGrouped(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintThreeActivitiesGrouped");
	ConstraintThreeActivitiesGrouped* cn=new ConstraintThreeActivitiesGrouped();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else if(xmlReader.name()=="Third_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->thirdActivityId=text.toInt();
			xmlReadingLog+="    Read third activity id="+CustomFETString::number(cn->thirdActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTwoActivitiesOrdered(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTwoActivitiesOrdered");
	ConstraintTwoActivitiesOrdered* cn=new ConstraintTwoActivitiesOrdered();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="First_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->firstActivityId=text.toInt();
			xmlReadingLog+="    Read first activity id="+CustomFETString::number(cn->firstActivityId)+"\n";
		}
		else if(xmlReader.name()=="Second_Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->secondActivityId=text.toInt();
			xmlReadingLog+="    Read second activity id="+CustomFETString::number(cn->secondActivityId)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readActivityPreferredTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredTimes");

	ConstraintActivityPreferredStartingTimes* cn=new ConstraintActivityPreferredStartingTimes();
	cn->nPreferredStartingTimes_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIMES; i++){
		cn->days[i] = cn->hours[i] = -1;
	}*/
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Times"){
			QString text=xmlReader.readElementText();
			cn->nPreferredStartingTimes_L=text.toInt();
			xmlReadingLog+="    Read number of preferred times="+CustomFETString::number(cn->nPreferredStartingTimes_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Time"){
			xmlReadingLog+="    Read: preferred time\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Day"){
					QString text=xmlReader.readElementText();
					cn->days_L.append(0);
					assert(cn->days_L.count()-1==i);
					for(cn->days_L[i]=0; cn->days_L[i]<this->nDaysPerWeek; cn->days_L[i]++)
						if(this->daysOfTheWeek[cn->days_L[i]]==text)
							break;

					if(cn->days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint ActivityPreferredTimes day corrupt for activity with id %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
		
					assert(cn->days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Hour"){
					QString text=xmlReader.readElementText();
					cn->hours_L.append(0);
					assert(cn->hours_L.count()-1==i);
					for(cn->hours_L[i]=0; cn->hours_L[i] < this->nHoursPerDay; cn->hours_L[i]++)
						if(this->hoursOfTheDay[cn->hours_L[i]]==text)
							break;
					
					if(cn->hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint ActivityPreferredTimes hour corrupt for activity with id %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->hours_L[i]>=0 && cn->hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			i++;

			if(!(i==cn->days_L.count()) || !(i==cn->hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->days_L.count());
			assert(i==cn->hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->nPreferredStartingTimes_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Times").arg("Preferred_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->nPreferredStartingTimes_L);
	return cn;
}

TimeConstraint* Rules::readActivityPreferredTimeSlots(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredTimeSlots");
	ConstraintActivityPreferredTimeSlots* cn=new ConstraintActivityPreferredTimeSlots();
	cn->p_nPreferredTimeSlots_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITY_PREFERRED_TIME_SLOTS; i++){
		cn->p_days[i] = cn->p_hours[i] = -1;
	}*/
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->p_activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->p_activityId)+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Time_Slots"){
			QString text=xmlReader.readElementText();
			cn->p_nPreferredTimeSlots_L=text.toInt();
			xmlReadingLog+="    Read number of preferred times="+CustomFETString::number(cn->p_nPreferredTimeSlots_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Time_Slot"){
			xmlReadingLog+="    Read: preferred time slot\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Day"){
					QString text=xmlReader.readElementText();
					cn->p_days_L.append(0);
					assert(cn->p_days_L.count()-1==i);
					for(cn->p_days_L[i]=0; cn->p_days_L[i]<this->nDaysPerWeek; cn->p_days_L[i]++)
						if(this->daysOfTheWeek[cn->p_days_L[i]]==text)
							break;

					if(cn->p_days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivityPreferredTimeSlots day corrupt for activity with id %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->p_activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
		
					assert(cn->p_days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->p_days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Hour"){
					QString text=xmlReader.readElementText();
					cn->p_hours_L.append(0);
					assert(cn->p_hours_L.count()-1==i);
					for(cn->p_hours_L[i]=0; cn->p_hours_L[i] < this->nHoursPerDay; cn->p_hours_L[i]++)
						if(this->hoursOfTheDay[cn->p_hours_L[i]]==text)
							break;
					
					if(cn->p_hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint ActivityPreferredTimeSlots hour corrupt for activity with id %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->p_activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->p_hours_L[i]>=0 && cn->p_hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->p_hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->p_days_L.count()) || !(i==cn->p_hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Time_Slot"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->p_days_L.count());
			assert(i==cn->p_hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->p_nPreferredTimeSlots_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Time_Slots").arg("Preferred_Time_Slot"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->p_nPreferredTimeSlots_L);
	return cn;
}

TimeConstraint* Rules::readActivityPreferredStartingTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredStartingTimes");
	ConstraintActivityPreferredStartingTimes* cn=new ConstraintActivityPreferredStartingTimes();
	cn->nPreferredStartingTimes_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITY_PREFERRED_STARTING_TIMES; i++){
		cn->days[i] = cn->hours[i] = -1;
	}*/
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Starting_Times"){
			QString text=xmlReader.readElementText();
			cn->nPreferredStartingTimes_L=text.toInt();
			xmlReadingLog+="    Read number of preferred starting times="+CustomFETString::number(cn->nPreferredStartingTimes_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Starting_Time"){
			xmlReadingLog+="    Read: preferred starting time\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Starting_Day"){
					QString text=xmlReader.readElementText();
					cn->days_L.append(0);
					assert(cn->days_L.count()-1==i);
					for(cn->days_L[i]=0; cn->days_L[i]<this->nDaysPerWeek; cn->days_L[i]++)
						if(this->daysOfTheWeek[cn->days_L[i]]==text)
							break;

					if(cn->days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivityPreferredStartingTimes day corrupt for activity with id %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred starting day="+this->daysOfTheWeek[cn->days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Starting_Hour"){
					QString text=xmlReader.readElementText();
					cn->hours_L.append(0);
					assert(cn->hours_L.count()-1==i);
					for(cn->hours_L[i]=0; cn->hours_L[i] < this->nHoursPerDay; cn->hours_L[i]++)
						if(this->hoursOfTheDay[cn->hours_L[i]]==text)
							break;
					
					if(cn->hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivityPreferredStartingTimes hour corrupt for activity with id %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->activityId)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->hours_L[i]>=0 && cn->hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred starting hour="+this->hoursOfTheDay[cn->hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->days_L.count()) || !(i==cn->hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Starting_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->days_L.count());
			assert(i==cn->hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->nPreferredStartingTimes_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Starting_Times").arg("Preferred_Starting_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->nPreferredStartingTimes_L);
	return cn;
}

TimeConstraint* Rules::readBreak(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintBreak");

	QList<int> days;
	QList<int> hours;
	double weightPercentage=100;
	int d=-1, h1=-1, h2=-1;
	bool active=true;
	QString comments=QString("");
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			weightPercentage=customFETStrToDouble(text);
			if(weightPercentage<0){
				xmlReader.raiseError(tr("Weight percentage incorrect"));
				return NULL;
			}
			assert(weightPercentage>=0);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			comments=text;
		}
		else if(xmlReader.name()=="Day"){
			QString text=xmlReader.readElementText();
			for(d=0; d<this->nDaysPerWeek; d++)
				if(this->daysOfTheWeek[d]==text)
					break;
			if(d>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint Break day corrupt for day %1 is inexistent ... ignoring constraint")
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(d<this->nDaysPerWeek);
			xmlReadingLog+="    Crt. day="+this->daysOfTheWeek[d]+"\n";
		}
		else if(xmlReader.name()=="Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1==this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is the last hour - impossible").arg(text));
				return NULL;
			}
			else if(h1>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"),
					tr("Constraint Break start hour corrupt for hour %1 is inexistent ... ignoring constraint")
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintTime;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Start hour="+this->hoursOfTheDay[h1]+"\n";
		}
		else if(xmlReader.name()=="End_Hour"){
			QString text=xmlReader.readElementText();
			for(h2=0; h2 < this->nHoursPerDay; h2++)
				if(this->hoursOfTheDay[h2]==text)
					break;
			if(h2==0){
				xmlReader.raiseError(tr("Hour %1 is the first hour - impossible").arg(text));
				return NULL;
			}
			else if(h2>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				return NULL;
			}
			/*if(h2<=0 || h2>this->nHoursPerDay){
				RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint Break end hour corrupt for hour %1 is inexistent ... ignoring constraint")
					.arg(text));
				//goto corruptConstraintTime;
				return NULL;
			}*/
			assert(h2>0 && h2 <= this->nHoursPerDay);
			xmlReadingLog+="    End hour="+this->hoursOfTheDay[h2]+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}

	assert(weightPercentage>=0);
	if(d<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Day"));
		return NULL;
	}
	else if(h1<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Start_Hour"));
		return NULL;
	}
	else if(h2<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("End_Hour"));
		return NULL;
	}
	assert(d>=0 && h1>=0 && h2>=0);

	ConstraintBreakTimes* cn = NULL;
	
	bool found=false;
	foreach(TimeConstraint* c, this->timeConstraintsList)
		if(c->type==CONSTRAINT_BREAK_TIMES){
			ConstraintBreakTimes* tna=(ConstraintBreakTimes*) c;
			if(true){
				found=true;
				
				for(int hh=h1; hh<h2; hh++){
					int k;
					for(k=0; k<tna->days.count(); k++)
						if(tna->days.at(k)==d && tna->hours.at(k)==hh)
							break;
					if(k==tna->days.count()){
						tna->days.append(d);
						tna->hours.append(hh);
					}
				}
				
				assert(tna->days.count()==tna->hours.count());
			}
		}
	if(!found){
		days.clear();
		hours.clear();
		for(int hh=h1; hh<h2; hh++){
			days.append(d);
			hours.append(hh);
		}
	
		cn=new ConstraintBreakTimes(weightPercentage, days, hours);
		cn->active=active;
		cn->comments=comments;

		return cn;
	}
	else
		return NULL;
}

TimeConstraint* Rules::readBreakTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintBreakTimes");
	ConstraintBreakTimes* cn=new ConstraintBreakTimes();
	int nNotAvailableSlots=-1;
	int i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}

		else if(xmlReader.name()=="Number_of_Break_Times"){
			QString text=xmlReader.readElementText();
			nNotAvailableSlots=text.toInt();
			xmlReadingLog+="    Read number of break times="+CustomFETString::number(nNotAvailableSlots)+"\n";
		}

		else if(xmlReader.name()=="Break_Time"){
			xmlReadingLog+="    Read: not available time\n";
			
			int d=-1;
			int h=-1;

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Day"){
					QString text=xmlReader.readElementText();
					for(d=0; d<this->nDaysPerWeek; d++)
						if(this->daysOfTheWeek[d]==text)
							break;

					if(d>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint BreakTimes day corrupt for day %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
		
					assert(d<this->nDaysPerWeek);
					xmlReadingLog+="    Day="+this->daysOfTheWeek[d]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Hour"){
					QString text=xmlReader.readElementText();
					for(h=0; h < this->nHoursPerDay; h++)
						if(this->hoursOfTheDay[h]==text)
							break;
					
					if(h>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint BreakTimes hour corrupt for hour %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(h>=0 && h < this->nHoursPerDay);
					xmlReadingLog+="    Hour="+this->hoursOfTheDay[h]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			i++;
			
			cn->days.append(d);
			cn->hours.append(h);

			if(d==-1 || h==-1){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Break_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==nNotAvailableSlots)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Break_Times").arg("Break_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==nNotAvailableSlots);
	return cn;
}

TimeConstraint* Rules::readTeachersNoGaps(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersNoGaps");
	ConstraintTeachersMaxGapsPerWeek* cn=new ConstraintTeachersMaxGapsPerWeek();
	cn->maxGaps=0;
	//ConstraintTeachersNoGaps* cn=new ConstraintTeachersNoGaps();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMaxGapsPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxGapsPerWeek");
	ConstraintTeachersMaxGapsPerWeek* cn=new ConstraintTeachersMaxGapsPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMaxGapsPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxGapsPerWeek");
	ConstraintTeacherMaxGapsPerWeek* cn=new ConstraintTeacherMaxGapsPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeachersMaxGapsPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxGapsPerDay");
	ConstraintTeachersMaxGapsPerDay* cn=new ConstraintTeachersMaxGapsPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readTeacherMaxGapsPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxGapsPerDay");
	ConstraintTeacherMaxGapsPerDay* cn=new ConstraintTeacherMaxGapsPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsNoGaps(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsNoGaps");

	ConstraintStudentsMaxGapsPerWeek* cn=new ConstraintStudentsMaxGapsPerWeek();
	
	cn->maxGaps=0;
	
	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetNoGaps(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetNoGaps");

	ConstraintStudentsSetMaxGapsPerWeek* cn=new ConstraintStudentsSetMaxGapsPerWeek();
	
	cn->maxGaps=0;
	
	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsMaxGapsPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxGapsPerWeek");
	ConstraintStudentsMaxGapsPerWeek* cn=new ConstraintStudentsMaxGapsPerWeek();

	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetMaxGapsPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxGapsPerWeek");
	ConstraintStudentsSetMaxGapsPerWeek* cn=new ConstraintStudentsSetMaxGapsPerWeek();
	
	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsMaxGapsPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxGapsPerDay");
	ConstraintStudentsMaxGapsPerDay* cn=new ConstraintStudentsMaxGapsPerDay();

	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetMaxGapsPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxGapsPerDay");
	ConstraintStudentsSetMaxGapsPerDay* cn=new ConstraintStudentsSetMaxGapsPerDay();
	
	//bool compulsory_read=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Gaps"){
			QString text=xmlReader.readElementText();
			cn->maxGaps=text.toInt();
			xmlReadingLog+="    Adding max gaps="+CustomFETString::number(cn->maxGaps)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
			//compulsory_read=true;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsEarly(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsEarly");
	ConstraintStudentsEarlyMaxBeginningsAtSecondHour* cn=new ConstraintStudentsEarlyMaxBeginningsAtSecondHour();
	
	cn->maxBeginningsAtSecondHour=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsEarlyMaxBeginningsAtSecondHour(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsEarlyMaxBeginningsAtSecondHour");
	ConstraintStudentsEarlyMaxBeginningsAtSecondHour* cn=new ConstraintStudentsEarlyMaxBeginningsAtSecondHour();
	cn->maxBeginningsAtSecondHour=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Beginnings_At_Second_Hour"){
			QString text=xmlReader.readElementText();
			cn->maxBeginningsAtSecondHour=text.toInt();
			if(!(cn->maxBeginningsAtSecondHour>=0)){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Max_Beginnings_At_Second_Hour"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Adding max beginnings at second hour="+CustomFETString::number(cn->maxBeginningsAtSecondHour)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(cn->maxBeginningsAtSecondHour>=0)){
		xmlReader.raiseError(tr("%1 not found").arg("Max_Beginnings_At_Second_Hour"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxBeginningsAtSecondHour>=0);
	return cn;
}

TimeConstraint* Rules::readStudentsSetEarly(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetEarly");
	ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* cn=new ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour();
	
	cn->maxBeginningsAtSecondHour=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

TimeConstraint* Rules::readStudentsSetEarlyMaxBeginningsAtSecondHour(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour");
	ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour* cn=new ConstraintStudentsSetEarlyMaxBeginningsAtSecondHour();
	cn->maxBeginningsAtSecondHour=-1;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Beginnings_At_Second_Hour"){
			QString text=xmlReader.readElementText();
			cn->maxBeginningsAtSecondHour=text.toInt();
			if(!(cn->maxBeginningsAtSecondHour>=0)){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Max_Beginnings_At_Second_Hour"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			xmlReadingLog+="    Adding max beginnings at second hour="+CustomFETString::number(cn->maxBeginningsAtSecondHour)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->students=text;
			xmlReadingLog+="    Read students name="+cn->students+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(cn->maxBeginningsAtSecondHour>=0)){
		xmlReader.raiseError(tr("%1 not found").arg("Max_Beginnings_At_Second_Hour"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(cn->maxBeginningsAtSecondHour>=0);
	return cn;
}

TimeConstraint* Rules::readActivitiesPreferredTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesPreferredTimes");

	ConstraintActivitiesPreferredStartingTimes* cn=new ConstraintActivitiesPreferredStartingTimes();
	cn->nPreferredStartingTimes_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES; i++){
		cn->days[i] = cn->hours[i] = -1;
	}*/
	cn->teacherName="";
	cn->studentsName="";
	cn->subjectName="";
	cn->activityTagName="";
	
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Times"){
			QString text=xmlReader.readElementText();
			cn->nPreferredStartingTimes_L=text.toInt();
			xmlReadingLog+="    Read number of preferred times="+CustomFETString::number(cn->nPreferredStartingTimes_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Time"){
			xmlReadingLog+="    Read: preferred time\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Day"){
					QString text=xmlReader.readElementText();
					cn->days_L.append(0);
					assert(cn->days_L.count()-1==i);
					for(cn->days_L[i]=0; cn->days_L[i]<this->nDaysPerWeek; cn->days_L[i]++)
						if(this->daysOfTheWeek[cn->days_L[i]]==text)
							break;
							
					if(cn->days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"),
							tr("Constraint ActivitiesPreferredTimes day corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, day %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Hour"){
					QString text=xmlReader.readElementText();
					cn->hours_L.append(0);
					assert(cn->hours_L.count()-1==i);
					for(cn->hours_L[i]=0; cn->hours_L[i] < this->nHoursPerDay; cn->hours_L[i]++)
						if(this->hoursOfTheDay[cn->hours_L[i]]==text)
							break;
							
					if(cn->hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredTimes hour corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, hour %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->hours_L[i]>=0 && cn->hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->days_L.count()) || !(i==cn->hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->days_L.count());
			assert(i==cn->hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->nPreferredStartingTimes_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Times").arg("Preferred_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->nPreferredStartingTimes_L);
	return cn;
}

TimeConstraint* Rules::readActivitiesPreferredTimeSlots(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesPreferredTimeSlots");
	ConstraintActivitiesPreferredTimeSlots* cn=new ConstraintActivitiesPreferredTimeSlots();
	cn->p_nPreferredTimeSlots_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITIES_PREFERRED_TIME_SLOTS; i++){
		cn->p_days[i] = cn->p_hours[i] = -1;
	}*/
	cn->p_teacherName="";
	cn->p_studentsName="";
	cn->p_subjectName="";
	cn->p_activityTagName="";
	
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->p_teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->p_teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->p_studentsName=text;
			xmlReadingLog+="    Read students name="+cn->p_studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->p_subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->p_subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->p_activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->p_activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->p_activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->p_activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Time_Slots"){
			QString text=xmlReader.readElementText();
			cn->p_nPreferredTimeSlots_L=text.toInt();
			xmlReadingLog+="    Read number of preferred times="+CustomFETString::number(cn->p_nPreferredTimeSlots_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Time_Slot"){
			xmlReadingLog+="    Read: preferred time slot\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Day"){
					QString text=xmlReader.readElementText();
					cn->p_days_L.append(0);
					assert(cn->p_days_L.count()-1==i);
					for(cn->p_days_L[i]=0; cn->p_days_L[i]<this->nDaysPerWeek; cn->p_days_L[i]++)
						if(this->daysOfTheWeek[cn->p_days_L[i]]==text)
							break;
							
					if(cn->p_days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredTimeSlots day corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, day %5 is inexistent ... ignoring constraint")
							.arg(cn->p_teacherName)
							.arg(cn->p_studentsName)
							.arg(cn->p_subjectName)
							.arg(cn->p_activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->p_days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->p_days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Hour"){
					QString text=xmlReader.readElementText();
					cn->p_hours_L.append(0);
					assert(cn->p_hours_L.count()-1==i);
					for(cn->p_hours_L[i]=0; cn->p_hours_L[i] < this->nHoursPerDay; cn->p_hours_L[i]++)
						if(this->hoursOfTheDay[cn->p_hours_L[i]]==text)
							break;
							
					if(cn->p_hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredTimeSlots hour corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, hour %5 is inexistent ... ignoring constraint")
							.arg(cn->p_teacherName)
							.arg(cn->p_studentsName)
							.arg(cn->p_subjectName)
							.arg(cn->p_activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->p_hours_L[i]>=0 && cn->p_hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->p_hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->p_days_L.count()) || !(i==cn->p_hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Time_Slot"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->p_days_L.count());
			assert(i==cn->p_hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->p_nPreferredTimeSlots_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Time_Slots").arg("Preferred_Time_Slot"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->p_nPreferredTimeSlots_L);
	return cn;
}

TimeConstraint* Rules::readActivitiesPreferredStartingTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesPreferredStartingTimes");
	ConstraintActivitiesPreferredStartingTimes* cn=new ConstraintActivitiesPreferredStartingTimes();
	cn->nPreferredStartingTimes_L=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_ACTIVITIES_PREFERRED_STARTING_TIMES; i++){
		cn->days[i] = cn->hours[i] = -1;
	}*/
	cn->teacherName="";
	cn->studentsName="";
	cn->subjectName="";
	cn->activityTagName="";
	
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Starting_Times"){
			QString text=xmlReader.readElementText();
			cn->nPreferredStartingTimes_L=text.toInt();
			xmlReadingLog+="    Read number of preferred starting times="+CustomFETString::number(cn->nPreferredStartingTimes_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Starting_Time"){
			xmlReadingLog+="    Read: preferred starting time\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Starting_Day"){
					QString text=xmlReader.readElementText();
					cn->days_L.append(0);
					assert(cn->days_L.count()-1==i);
					for(cn->days_L[i]=0; cn->days_L[i]<this->nDaysPerWeek; cn->days_L[i]++)
						if(this->daysOfTheWeek[cn->days_L[i]]==text)
							break;
							
					if(cn->days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredStartingTimes day corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, day %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred starting day="+this->daysOfTheWeek[cn->days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Starting_Hour"){
					QString text=xmlReader.readElementText();
					cn->hours_L.append(0);
					assert(cn->hours_L.count()-1==i);
					for(cn->hours_L[i]=0; cn->hours_L[i] < this->nHoursPerDay; cn->hours_L[i]++)
						if(this->hoursOfTheDay[cn->hours_L[i]]==text)
							break;
							
					if(cn->hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredStartingTimes hour corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, hour %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->hours_L[i]>=0 && cn->hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred starting hour="+this->hoursOfTheDay[cn->hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->days_L.count()) || !(i==cn->hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Starting_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->days_L.count());
			assert(i==cn->hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->nPreferredStartingTimes_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Starting_Times").arg("Preferred_Starting_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->nPreferredStartingTimes_L);
	return cn;
}

////////////////
TimeConstraint* Rules::readSubactivitiesPreferredTimeSlots(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubactivitiesPreferredTimeSlots");
	ConstraintSubactivitiesPreferredTimeSlots* cn=new ConstraintSubactivitiesPreferredTimeSlots();
	cn->p_nPreferredTimeSlots_L=0;
	cn->componentNumber=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_SUBACTIVITIES_PREFERRED_TIME_SLOTS; i++){
		cn->p_days[i] = cn->p_hours[i] = -1;
	}*/
	cn->p_teacherName="";
	cn->p_studentsName="";
	cn->p_subjectName="";
	cn->p_activityTagName="";
	
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Component_Number"){
			QString text=xmlReader.readElementText();
			cn->componentNumber=text.toInt();
			xmlReadingLog+="    Adding component number="+CustomFETString::number(cn->componentNumber)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->p_teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->p_teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->p_studentsName=text;
			xmlReadingLog+="    Read students name="+cn->p_studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->p_subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->p_subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->p_activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->p_activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->p_activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->p_activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Time_Slots"){
			QString text=xmlReader.readElementText();
			cn->p_nPreferredTimeSlots_L=text.toInt();
			xmlReadingLog+="    Read number of preferred times="+CustomFETString::number(cn->p_nPreferredTimeSlots_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Time_Slot"){
			xmlReadingLog+="    Read: preferred time slot\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Day"){
					QString text=xmlReader.readElementText();
					cn->p_days_L.append(0);
					assert(cn->p_days_L.count()-1==i);
					for(cn->p_days_L[i]=0; cn->p_days_L[i]<this->nDaysPerWeek; cn->p_days_L[i]++)
						if(this->daysOfTheWeek[cn->p_days_L[i]]==text)
							break;
							
					if(cn->p_days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredTimeSlots day corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, day %5 is inexistent ... ignoring constraint")
							.arg(cn->p_teacherName)
							.arg(cn->p_studentsName)
							.arg(cn->p_subjectName)
							.arg(cn->p_activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->p_days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred day="+this->daysOfTheWeek[cn->p_days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Hour"){
					QString text=xmlReader.readElementText();
					cn->p_hours_L.append(0);
					assert(cn->p_hours_L.count()-1==i);
					for(cn->p_hours_L[i]=0; cn->p_hours_L[i] < this->nHoursPerDay; cn->p_hours_L[i]++)
						if(this->hoursOfTheDay[cn->p_hours_L[i]]==text)
							break;
							
					if(cn->p_hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredTimeSlots hour corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, hour %5 is inexistent ... ignoring constraint")
							.arg(cn->p_teacherName)
							.arg(cn->p_studentsName)
							.arg(cn->p_subjectName)
							.arg(cn->p_activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->p_hours_L[i]>=0 && cn->p_hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred hour="+this->hoursOfTheDay[cn->p_hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->p_days_L.count()) || !(i==cn->p_hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Time_Slot"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->p_days_L.count());
			assert(i==cn->p_hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->p_nPreferredTimeSlots_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Time_Slots").arg("Preferred_Time_Slot"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->p_nPreferredTimeSlots_L);
	return cn;
}

TimeConstraint* Rules::readSubactivitiesPreferredStartingTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubactivitiesPreferredStartingTimes");
	ConstraintSubactivitiesPreferredStartingTimes* cn=new ConstraintSubactivitiesPreferredStartingTimes();
	cn->nPreferredStartingTimes_L=0;
	cn->componentNumber=0;
	int i;
	/*for(i=0; i<MAX_N_CONSTRAINT_SUBACTIVITIES_PREFERRED_STARTING_TIMES; i++){
		cn->days[i] = cn->hours[i] = -1;
	}*/
	cn->teacherName="";
	cn->studentsName="";
	cn->subjectName="";
	cn->activityTagName="";
	
	i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Component_Number"){
			QString text=xmlReader.readElementText();
			cn->componentNumber=text.toInt();
			xmlReadingLog+="    Adding component number="+CustomFETString::number(cn->componentNumber)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Teacher_Name"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Students_Name"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Subject_Name"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject name="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag_Name"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag name="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Starting_Times"){
			QString text=xmlReader.readElementText();
			cn->nPreferredStartingTimes_L=text.toInt();
			xmlReadingLog+="    Read number of preferred starting times="+CustomFETString::number(cn->nPreferredStartingTimes_L)+"\n";
		}
		else if(xmlReader.name()=="Preferred_Starting_Time"){
			xmlReadingLog+="    Read: preferred starting time\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Preferred_Starting_Day"){
					QString text=xmlReader.readElementText();
					cn->days_L.append(0);
					assert(cn->days_L.count()-1==i);
					for(cn->days_L[i]=0; cn->days_L[i]<this->nDaysPerWeek; cn->days_L[i]++)
						if(this->daysOfTheWeek[cn->days_L[i]]==text)
							break;
							
					if(cn->days_L[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredStartingTimes day corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, day %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->days_L[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Preferred starting day="+this->daysOfTheWeek[cn->days_L[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Preferred_Starting_Hour"){
					QString text=xmlReader.readElementText();
					cn->hours_L.append(0);
					assert(cn->hours_L.count()-1==i);
					for(cn->hours_L[i]=0; cn->hours_L[i] < this->nHoursPerDay; cn->hours_L[i]++)
						if(this->hoursOfTheDay[cn->hours_L[i]]==text)
							break;
							
					if(cn->hours_L[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesPreferredStartingTimes hour corrupt for teacher name=%1, students names=%2, subject name=%3, activity tag name=%4, hour %5 is inexistent ... ignoring constraint")
							.arg(cn->teacherName)
							.arg(cn->studentsName)
							.arg(cn->subjectName)
							.arg(cn->activityTagName)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->hours_L[i]>=0 && cn->hours_L[i] < this->nHoursPerDay);
					xmlReadingLog+="    Preferred starting hour="+this->hoursOfTheDay[cn->hours_L[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->days_L.count()) || !(i==cn->hours_L.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Preferred_Starting_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->days_L.count());
			assert(i==cn->hours_L.count());
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(i==cn->nPreferredStartingTimes_L)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Starting_Times").arg("Preferred_Starting_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==cn->nPreferredStartingTimes_L);
	return cn;
}

//2011-09-25
TimeConstraint* Rules::readActivitiesOccupyMaxTimeSlotsFromSelection(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesOccupyMaxTimeSlotsFromSelection");
	ConstraintActivitiesOccupyMaxTimeSlotsFromSelection* cn=new ConstraintActivitiesOccupyMaxTimeSlotsFromSelection();
	
	int ac=0;
	int tsc=0;
	int i=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";

		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			ac=text.toInt();
			xmlReadingLog+="    Read number of activities="+CustomFETString::number(ac)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesIds.append(text.toInt());
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesIds[cn->activitiesIds.count()-1])+"\n";
		}
		else if(xmlReader.name()=="Number_of_Selected_Time_Slots"){
			QString text=xmlReader.readElementText();
			tsc=text.toInt();
			xmlReadingLog+="    Read number of selected time slots="+CustomFETString::number(tsc)+"\n";
		}
		else if(xmlReader.name()=="Selected_Time_Slot"){
			xmlReadingLog+="    Read: selected time slot\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Selected_Day"){
					QString text=xmlReader.readElementText();
					cn->selectedDays.append(0);
					assert(cn->selectedDays.count()-1==i);
					for(cn->selectedDays[i]=0; cn->selectedDays[i]<this->nDaysPerWeek; cn->selectedDays[i]++)
						if(this->daysOfTheWeek[cn->selectedDays[i]]==text)
							break;
							
					if(cn->selectedDays[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesOccupyMaxTimeSlotsFromSelection day corrupt, day %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->selectedDays[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Selected day="+this->daysOfTheWeek[cn->selectedDays[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Selected_Hour"){
					QString text=xmlReader.readElementText();
					cn->selectedHours.append(0);
					assert(cn->selectedHours.count()-1==i);
					for(cn->selectedHours[i]=0; cn->selectedHours[i] < this->nHoursPerDay; cn->selectedHours[i]++)
						if(this->hoursOfTheDay[cn->selectedHours[i]]==text)
							break;
							
					if(cn->selectedHours[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr(" Constraint ActivitiesOccupyMaxTimeSlotsFromSelection hour corrupt, hour %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->selectedHours[i]>=0 && cn->selectedHours[i] < this->nHoursPerDay);
					xmlReadingLog+="    Selected hour="+this->hoursOfTheDay[cn->selectedHours[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;
			
			if(!(i==cn->selectedDays.count()) || !(i==cn->selectedHours.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Selected_Time_Slot"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->selectedDays.count());
			assert(i==cn->selectedHours.count());
		}
		else if(xmlReader.name()=="Max_Number_of_Occupied_Time_Slots"){
			QString text=xmlReader.readElementText();
			cn->maxOccupiedTimeSlots=text.toInt();
			xmlReadingLog+="    Read max number of occupied time slots="+CustomFETString::number(cn->maxOccupiedTimeSlots)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(!(ac==cn->activitiesIds.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}

	if(!(i==tsc)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Selected_Time_Slots").arg("Selected_Time_Slot"));
		delete cn;
		cn=NULL;
		return NULL;
	}

	assert(ac==cn->activitiesIds.count());
	assert(i==tsc);
	return cn;
}
////////////////

//2011-09-30
TimeConstraint* Rules::readActivitiesMaxSimultaneousInSelectedTimeSlots(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots");
	ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots* cn=new ConstraintActivitiesMaxSimultaneousInSelectedTimeSlots();
	
	int ac=0;
	int tsc=0;
	int i=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";

		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			ac=text.toInt();
			xmlReadingLog+="    Read number of activities="+CustomFETString::number(ac)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesIds.append(text.toInt());
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesIds[cn->activitiesIds.count()-1])+"\n";
		}
		else if(xmlReader.name()=="Number_of_Selected_Time_Slots"){
			QString text=xmlReader.readElementText();
			tsc=text.toInt();
			xmlReadingLog+="    Read number of selected time slots="+CustomFETString::number(tsc)+"\n";
		}
		else if(xmlReader.name()=="Selected_Time_Slot"){
			xmlReadingLog+="    Read: selected time slot\n";

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Selected_Day"){
					QString text=xmlReader.readElementText();
					cn->selectedDays.append(0);
					assert(cn->selectedDays.count()-1==i);
					for(cn->selectedDays[i]=0; cn->selectedDays[i]<this->nDaysPerWeek; cn->selectedDays[i]++)
						if(this->daysOfTheWeek[cn->selectedDays[i]]==text)
							break;
							
					if(cn->selectedDays[i]>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint ActivitiesMaxSimultaneousInSelectedTimeSlots day corrupt, day %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
							
					assert(cn->selectedDays[i]<this->nDaysPerWeek);
					xmlReadingLog+="    Selected day="+this->daysOfTheWeek[cn->selectedDays[i]]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Selected_Hour"){
					QString text=xmlReader.readElementText();
					cn->selectedHours.append(0);
					assert(cn->selectedHours.count()-1==i);
					for(cn->selectedHours[i]=0; cn->selectedHours[i] < this->nHoursPerDay; cn->selectedHours[i]++)
						if(this->hoursOfTheDay[cn->selectedHours[i]]==text)
							break;
							
					if(cn->selectedHours[i]>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr(" Constraint ActivitiesMaxSimultaneousInSelectedTimeSlots hour corrupt, hour %1 is inexistent ... ignoring constraint")
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintTime;
						return NULL;
					}
					
					assert(cn->selectedHours[i]>=0 && cn->selectedHours[i] < this->nHoursPerDay);
					xmlReadingLog+="    Selected hour="+this->hoursOfTheDay[cn->selectedHours[i]]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}

			i++;

			if(!(i==cn->selectedDays.count()) || !(i==cn->selectedHours.count())){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Selected_Time_Slot"));
				delete cn;
				cn=NULL;
				return NULL;
			}
			assert(i==cn->selectedDays.count());
			assert(i==cn->selectedHours.count());
		}
		else if(xmlReader.name()=="Max_Number_of_Simultaneous_Activities"){
			QString text=xmlReader.readElementText();
			cn->maxSimultaneous=text.toInt();
			xmlReadingLog+="    Read max number of simultaneous activities="+CustomFETString::number(cn->maxSimultaneous)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(!(ac==cn->activitiesIds.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}

	if(!(i==tsc)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Selected_Time_Slots").arg("Selected_Time_Slot"));
		delete cn;
		cn=NULL;
		return NULL;
	}

	assert(ac==cn->activitiesIds.count());
	assert(i==tsc);
	return cn;
}
////////////////

///space constraints reading routines
SpaceConstraint* Rules::readBasicCompulsorySpace(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintBasicCompulsorySpace");
	ConstraintBasicCompulsorySpace* cn=new ConstraintBasicCompulsorySpace();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";

		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		/*if(xmlReader.name()=="Weight"){
			cn->weight=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight="+CustomFETString::number(cn->weight)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			if(text=="yes"){
				cn->compulsory=true;
				xmlReadingLog+="    Current constraint is compulsory\n";
			}
			else{
				cn->compulsory=false;
				xmlReadingLog+="    Current constraint is not compulsory\n";
			}
		}*/
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readRoomNotAvailable(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintRoomNotAvailable");

	QList<int> days;
	QList<int> hours;
	QString room;
	double weightPercentage=100;
	int d=-1, h1=-1, h2=-1;
	bool active=true;
	QString comments=QString("");
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			comments=text;
		}
		else if(xmlReader.name()=="Day"){
			QString text=xmlReader.readElementText();
			for(d=0; d<this->nDaysPerWeek; d++)
				if(this->daysOfTheWeek[d]==text)
					break;
			if(d>=this->nDaysPerWeek){
				xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint RoomNotAvailable day corrupt for room %1, day %2 is inexistent ... ignoring constraint")
					.arg(room)
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintSpace;
				return NULL;
			}
			assert(d<this->nDaysPerWeek);
			xmlReadingLog+="    Crt. day="+this->daysOfTheWeek[d]+"\n";
		}
		else if(xmlReader.name()=="Start_Hour"){
			QString text=xmlReader.readElementText();
			for(h1=0; h1 < this->nHoursPerDay; h1++)
				if(this->hoursOfTheDay[h1]==text)
					break;
			if(h1==this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is the last hour - impossible").arg(text));
				return NULL;
			}
			else if(h1>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint RoomNotAvailable start hour corrupt for room %1, hour %2 is inexistent ... ignoring constraint")
					.arg(room)
					.arg(text));*/
				//cn=NULL;
				//goto corruptConstraintSpace;
				return NULL;
			}
			assert(h1>=0 && h1 < this->nHoursPerDay);
			xmlReadingLog+="    Start hour="+this->hoursOfTheDay[h1]+"\n";
		}
		else if(xmlReader.name()=="End_Hour"){
			QString text=xmlReader.readElementText();
			for(h2=0; h2 < this->nHoursPerDay; h2++)
				if(this->hoursOfTheDay[h2]==text)
					break;
			if(h2==0){
				xmlReader.raiseError(tr("Hour %1 is the first hour - impossible").arg(text));
				return NULL;
			}
			else if(h2<0 || h2>this->nHoursPerDay){
				xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
				/*RulesReconcilableMessage::information(parent, tr("FET information"), 
					tr("Constraint RoomNotAvailable end hour corrupt for room %1, hour %2 is inexistent ... ignoring constraint")
					.arg(room)
					.arg(text));*/
				//goto corruptConstraintSpace;
				return NULL;
			}
			assert(h2>0 && h2 <= this->nHoursPerDay);
			xmlReadingLog+="    End hour="+this->hoursOfTheDay[h2]+"\n";
		}
		else if(xmlReader.name()=="Room_Name"){
			QString text=xmlReader.readElementText();
			room=text;
			xmlReadingLog+="    Read room name="+room+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(d<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Day"));
		return NULL;
	}
	else if(h1<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("Start_Hour"));
		return NULL;
	}
	else if(h2<0){
		xmlReader.raiseError(tr("Field missing: %1").arg("End_Hour"));
		return NULL;
	}
	assert(weightPercentage>=0);
	assert(d>=0 && h1>=0 && h2>=0);

	ConstraintRoomNotAvailableTimes* cn = NULL;
	
	bool found=false;
	foreach(SpaceConstraint* c, this->spaceConstraintsList)
		if(c->type==CONSTRAINT_ROOM_NOT_AVAILABLE_TIMES){
			ConstraintRoomNotAvailableTimes* tna=(ConstraintRoomNotAvailableTimes*) c;
			if(tna->room==room){
				found=true;
				
				for(int hh=h1; hh<h2; hh++){
					int k;
					for(k=0; k<tna->days.count(); k++)
						if(tna->days.at(k)==d && tna->hours.at(k)==hh)
							break;
					if(k==tna->days.count()){
						tna->days.append(d);
						tna->hours.append(hh);
					}
				}
				
				assert(tna->days.count()==tna->hours.count());
			}
		}
	if(!found){
		days.clear();
		hours.clear();
		for(int hh=h1; hh<h2; hh++){
			days.append(d);
			hours.append(hh);
		}
	
		cn=new ConstraintRoomNotAvailableTimes(weightPercentage, room, days, hours);
		cn->active=active;
		cn->comments=comments;

		return cn;
	}
	else
		return NULL;
}

SpaceConstraint* Rules::readRoomNotAvailableTimes(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintRoomNotAvailableTimes");
	ConstraintRoomNotAvailableTimes* cn=new ConstraintRoomNotAvailableTimes();
	int nNotAvailableSlots=-1;
	int i=0;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Read weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}

		else if(xmlReader.name()=="Number_of_Not_Available_Times"){
			QString text=xmlReader.readElementText();
			nNotAvailableSlots=text.toInt();
			xmlReadingLog+="    Read number of not available times="+CustomFETString::number(nNotAvailableSlots)+"\n";
		}

		else if(xmlReader.name()=="Not_Available_Time"){
			xmlReadingLog+="    Read: not available time\n";
			
			int d=-1;
			int h=-1;

			assert(xmlReader.isStartElement());
			while(xmlReader.readNextStartElement()){
				xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
				if(xmlReader.name()=="Day"){
					QString text=xmlReader.readElementText();
					for(d=0; d<this->nDaysPerWeek; d++)
						if(this->daysOfTheWeek[d]==text)
							break;

					if(d>=this->nDaysPerWeek){
						xmlReader.raiseError(tr("Day %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint RoomNotAvailableTimes day corrupt for room %1, day %2 is inexistent ... ignoring constraint")
							.arg(cn->room)
							.arg(text));*/
						delete cn;
						cn=NULL;
						return NULL;
						//goto corruptConstraintSpace;
					}
		
					assert(d<this->nDaysPerWeek);
					xmlReadingLog+="    Day="+this->daysOfTheWeek[d]+"("+CustomFETString::number(i)+")"+"\n";
				}
				else if(xmlReader.name()=="Hour"){
					QString text=xmlReader.readElementText();
					for(h=0; h < this->nHoursPerDay; h++)
						if(this->hoursOfTheDay[h]==text)
							break;
					
					if(h>=this->nHoursPerDay){
						xmlReader.raiseError(tr("Hour %1 is inexistent").arg(text));
						/*RulesReconcilableMessage::information(parent, tr("FET information"), 
							tr("Constraint RoomNotAvailableTimes hour corrupt for room %1, hour %2 is inexistent ... ignoring constraint")
							.arg(cn->room)
							.arg(text));*/
						delete cn;
						cn=NULL;
						//goto corruptConstraintSpace;
						return NULL;
					}
					
					assert(h>=0 && h < this->nHoursPerDay);
					xmlReadingLog+="    Hour="+this->hoursOfTheDay[h]+"\n";
				}
				else{
					xmlReader.skipCurrentElement();
					xmlReaderNumberOfUnrecognizedFields++;
				}
			}
			i++;
			
			cn->days.append(d);
			cn->hours.append(h);

			if(d==-1 || h==-1){
				xmlReader.raiseError(tr("%1 is incorrect").arg("Not_Available_Time"));
				delete cn;
				cn=NULL;
				return NULL;
			}
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->room=text;
			xmlReadingLog+="    Read room name="+cn->room+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	assert(i==cn->days.count() && i==cn->hours.count());
	if(!(i==nNotAvailableSlots)){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Not_Available_Times").arg("Not_Available_Time"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(i==nNotAvailableSlots);
	return cn;
}

SpaceConstraint* Rules::readActivityPreferredRoom(QWidget* parent, QXmlStreamReader& xmlReader, FakeString& xmlReadingLog,
bool& reportUnspecifiedPermanentlyLockedSpace){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredRoom");
	ConstraintActivityPreferredRoom* cn=new ConstraintActivityPreferredRoom();
	cn->permanentlyLocked=false; //default
	bool foundLocked=false;
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Permanently_Locked"){
			QString text=xmlReader.readElementText();
			if(text=="true" || text=="1" || text=="yes"){
				xmlReadingLog+="    Permanently locked\n";
				cn->permanentlyLocked=true;
			}
			else{
				if(!(text=="no" || text=="false" || text=="0")){
					RulesReconcilableMessage::warning(parent, tr("FET warning"),
						tr("Found constraint activity preferred room with tag permanently locked"
						" which is not 'true', 'false', 'yes', 'no', '1' or '0'."
						" The tag will be considered false",
						"Instructions for translators: please leave the 'true', 'false', 'yes' and 'no' fields untranslated, as they are in English"));
				}
				//assert(text=="false" || text=="0" || text=="no");
				xmlReadingLog+="    Not permanently locked\n";
				cn->permanentlyLocked=false;
			}
			foundLocked=true;
		}

		/*if(xmlReader.name()=="Weight"){
			cn->weight=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight="+CustomFETString::number(cn->weight)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			if(text=="yes"){
				cn->compulsory=true;
				xmlReadingLog+="    Current constraint is compulsory\n";
			}
			else{
				cn->compulsory=false;
				xmlReadingLog+="    Current constraint is not compulsory\n";
			}
		}*/
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!foundLocked && reportUnspecifiedPermanentlyLockedSpace){
		int t=RulesReconcilableMessage::information(parent, tr("FET information"),
			tr("Found constraint activity preferred room, with unspecified tag"
			" 'permanently locked' - this tag will be set to 'false' by default. You can always modify it"
			" by editing the constraint in the 'Data' menu")+"\n\n"
			+tr("Explanation: starting with version 5.8.0 (January 2009), the constraint"
			" activity preferred room has"
			" a new tag, 'permanently locked' (true or false)."
			" It is recommended to make the tag 'permanently locked' true for the constraints you"
			" need to be not modifiable from the 'Timetable' menu"
			" and leave this tag false for the constraints you need to be modifiable from the 'Timetable' menu"
			" (the 'permanently locked' tag can be modified by editing the constraint from the 'Data' menu)."
			" This way, when viewing the timetable"
			" and locking/unlocking some activities, you will not unlock the constraints which"
			" need to be locked all the time."
			),
			tr("Skip rest"), tr("See next"), QString(), 1, 0 );
		if(t==0)
			reportUnspecifiedPermanentlyLockedSpace=false;
	}

	return cn;
}

SpaceConstraint* Rules::readActivityPreferredRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityPreferredRooms");
	int _n_preferred_rooms=0;
	ConstraintActivityPreferredRooms* cn=new ConstraintActivityPreferredRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		/*if(xmlReader.name()=="Weight"){
			cn->weight=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight="+CustomFETString::number(cn->weight)+"\n";
		}
		else if(xmlReader.name()=="Compulsory"){
			if(text=="yes"){
				cn->compulsory=true;
				xmlReadingLog+="    Current constraint is compulsory\n";
			}
			else{
				cn->compulsory=false;
				xmlReadingLog+="    Current constraint is not compulsory\n";
			}
		}*/
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activityId=text.toInt();
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activityId)+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readSubjectPreferredRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectPreferredRoom");
	ConstraintSubjectPreferredRoom* cn=new ConstraintSubjectPreferredRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readSubjectPreferredRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectPreferredRooms");
	int _n_preferred_rooms=0;
	ConstraintSubjectPreferredRooms* cn=new ConstraintSubjectPreferredRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readSubjectSubjectTagPreferredRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectSubjectTagPreferredRoom");
	ConstraintSubjectActivityTagPreferredRoom* cn=new ConstraintSubjectActivityTagPreferredRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readSubjectSubjectTagPreferredRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectSubjectTagPreferredRooms");
	int _n_preferred_rooms=0;
	ConstraintSubjectActivityTagPreferredRooms* cn=new ConstraintSubjectActivityTagPreferredRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Subject_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readSubjectActivityTagPreferredRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectActivityTagPreferredRoom");
	ConstraintSubjectActivityTagPreferredRoom* cn=new ConstraintSubjectActivityTagPreferredRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readSubjectActivityTagPreferredRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintSubjectActivityTagPreferredRooms");
	int _n_preferred_rooms=0;
	ConstraintSubjectActivityTagPreferredRooms* cn=new ConstraintSubjectActivityTagPreferredRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight"){
			//cn->weight=customFETStrToDouble(text);
			xmlReader.skipCurrentElement();
			xmlReadingLog+="    Ignoring old tag - weight - making weight percentage=100\n";
			cn->weightPercentage=100;
		}
		else if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Compulsory"){
			QString text=xmlReader.readElementText();
			if(text=="yes"){
				//cn->compulsory=true;
				xmlReadingLog+="    Ignoring old tag - Current constraint is compulsory\n";
				cn->weightPercentage=100;
			}
			else{
				//cn->compulsory=false;
				xmlReadingLog+="    Old tag - current constraint is not compulsory - making weightPercentage=0%\n";
				cn->weightPercentage=0;
			}
		}
		else if(xmlReader.name()=="Subject"){
			QString text=xmlReader.readElementText();
			cn->subjectName=text;
			xmlReadingLog+="    Read subject="+cn->subjectName+"\n";
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readActivityTagPreferredRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	//added 6 apr 2009
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityTagPreferredRoom");
	ConstraintActivityTagPreferredRoom* cn=new ConstraintActivityTagPreferredRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readActivityTagPreferredRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivityTagPreferredRooms");
	int _n_preferred_rooms=0;
	ConstraintActivityTagPreferredRooms* cn=new ConstraintActivityTagPreferredRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Activity_Tag"){
			QString text=xmlReader.readElementText();
			cn->activityTagName=text;
			xmlReadingLog+="    Read activity tag="+cn->activityTagName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readStudentsSetHomeRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetHomeRoom");
	ConstraintStudentsSetHomeRoom* cn=new ConstraintStudentsSetHomeRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsSetHomeRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetHomeRooms");
	int _n_preferred_rooms=0;
	ConstraintStudentsSetHomeRooms* cn=new ConstraintStudentsSetHomeRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readTeacherHomeRoom(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherHomeRoom");
	ConstraintTeacherHomeRoom* cn=new ConstraintTeacherHomeRoom();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Room"){
			QString text=xmlReader.readElementText();
			cn->roomName=text;
			xmlReadingLog+="    Read room="+cn->roomName+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeacherHomeRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherHomeRooms");
	int _n_preferred_rooms=0;
	ConstraintTeacherHomeRooms* cn=new ConstraintTeacherHomeRooms();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Number_of_Preferred_Rooms"){
			QString text=xmlReader.readElementText();
			_n_preferred_rooms=text.toInt();
			xmlReadingLog+="    Read number of preferred rooms: "+CustomFETString::number(_n_preferred_rooms)+"\n";
			//assert(_n_preferred_rooms>=2);
		}
		else if(xmlReader.name()=="Preferred_Room"){
			QString text=xmlReader.readElementText();
			cn->roomsNames.append(text);
			xmlReadingLog+="    Read room="+text+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	if(!(_n_preferred_rooms==cn->roomsNames.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Preferred_Rooms").arg("Preferred_Room"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(_n_preferred_rooms==cn->roomsNames.count());
	return cn;
}

SpaceConstraint* Rules::readTeacherMaxBuildingChangesPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxBuildingChangesPerDay");
	ConstraintTeacherMaxBuildingChangesPerDay* cn=new ConstraintTeacherMaxBuildingChangesPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Day"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerDay=text.toInt();
			xmlReadingLog+="    Max. building changes per day="+CustomFETString::number(cn->maxBuildingChangesPerDay)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeachersMaxBuildingChangesPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxBuildingChangesPerDay");
	ConstraintTeachersMaxBuildingChangesPerDay* cn=new ConstraintTeachersMaxBuildingChangesPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Day"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerDay=text.toInt();
			xmlReadingLog+="    Max. building changes per day="+CustomFETString::number(cn->maxBuildingChangesPerDay)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeacherMaxBuildingChangesPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMaxBuildingChangesPerWeek");
	ConstraintTeacherMaxBuildingChangesPerWeek* cn=new ConstraintTeacherMaxBuildingChangesPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerWeek=text.toInt();
			xmlReadingLog+="    Max. building changes per week="+CustomFETString::number(cn->maxBuildingChangesPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeachersMaxBuildingChangesPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMaxBuildingChangesPerWeek");
	ConstraintTeachersMaxBuildingChangesPerWeek* cn=new ConstraintTeachersMaxBuildingChangesPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerWeek=text.toInt();
			xmlReadingLog+="    Max. building changes per week="+CustomFETString::number(cn->maxBuildingChangesPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeacherMinGapsBetweenBuildingChanges(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeacherMinGapsBetweenBuildingChanges");
	ConstraintTeacherMinGapsBetweenBuildingChanges* cn=new ConstraintTeacherMinGapsBetweenBuildingChanges();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Teacher"){
			QString text=xmlReader.readElementText();
			cn->teacherName=text;
			xmlReadingLog+="    Read teacher name="+cn->teacherName+"\n";
		}
		else if(xmlReader.name()=="Min_Gaps_Between_Building_Changes"){
			QString text=xmlReader.readElementText();
			cn->minGapsBetweenBuildingChanges=text.toInt();
			xmlReadingLog+="    Min gaps between building changes="+CustomFETString::number(cn->minGapsBetweenBuildingChanges)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readTeachersMinGapsBetweenBuildingChanges(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintTeachersMinGapsBetweenBuildingChanges");
	ConstraintTeachersMinGapsBetweenBuildingChanges* cn=new ConstraintTeachersMinGapsBetweenBuildingChanges();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Min_Gaps_Between_Building_Changes"){
			QString text=xmlReader.readElementText();
			cn->minGapsBetweenBuildingChanges=text.toInt();
			xmlReadingLog+="    Min gaps between building changes="+CustomFETString::number(cn->minGapsBetweenBuildingChanges)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsSetMaxBuildingChangesPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxBuildingChangesPerDay");
	ConstraintStudentsSetMaxBuildingChangesPerDay* cn=new ConstraintStudentsSetMaxBuildingChangesPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Day"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerDay=text.toInt();
			xmlReadingLog+="    Max. building changes per day="+CustomFETString::number(cn->maxBuildingChangesPerDay)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsMaxBuildingChangesPerDay(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxBuildingChangesPerDay");
	ConstraintStudentsMaxBuildingChangesPerDay* cn=new ConstraintStudentsMaxBuildingChangesPerDay();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Day"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerDay=text.toInt();
			xmlReadingLog+="    Max. building changes per day="+CustomFETString::number(cn->maxBuildingChangesPerDay)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsSetMaxBuildingChangesPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMaxBuildingChangesPerWeek");
	ConstraintStudentsSetMaxBuildingChangesPerWeek* cn=new ConstraintStudentsSetMaxBuildingChangesPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerWeek=text.toInt();
			xmlReadingLog+="    Max. building changes per week="+CustomFETString::number(cn->maxBuildingChangesPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsMaxBuildingChangesPerWeek(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMaxBuildingChangesPerWeek");
	ConstraintStudentsMaxBuildingChangesPerWeek* cn=new ConstraintStudentsMaxBuildingChangesPerWeek();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Max_Building_Changes_Per_Week"){
			QString text=xmlReader.readElementText();
			cn->maxBuildingChangesPerWeek=text.toInt();
			xmlReadingLog+="    Max. building changes per week="+CustomFETString::number(cn->maxBuildingChangesPerWeek)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsSetMinGapsBetweenBuildingChanges(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsSetMinGapsBetweenBuildingChanges");
	ConstraintStudentsSetMinGapsBetweenBuildingChanges* cn=new ConstraintStudentsSetMinGapsBetweenBuildingChanges();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Students"){
			QString text=xmlReader.readElementText();
			cn->studentsName=text;
			xmlReadingLog+="    Read students name="+cn->studentsName+"\n";
		}
		else if(xmlReader.name()=="Min_Gaps_Between_Building_Changes"){
			QString text=xmlReader.readElementText();
			cn->minGapsBetweenBuildingChanges=text.toInt();
			xmlReadingLog+="    min gaps between building changes="+CustomFETString::number(cn->minGapsBetweenBuildingChanges)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

SpaceConstraint* Rules::readStudentsMinGapsBetweenBuildingChanges(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintStudentsMinGapsBetweenBuildingChanges");
	ConstraintStudentsMinGapsBetweenBuildingChanges* cn=new ConstraintStudentsMinGapsBetweenBuildingChanges();
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";
		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Min_Gaps_Between_Building_Changes"){
			QString text=xmlReader.readElementText();
			cn->minGapsBetweenBuildingChanges=text.toInt();
			xmlReadingLog+="    min gaps between building changes="+CustomFETString::number(cn->minGapsBetweenBuildingChanges)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	return cn;
}

//2012-04-29
SpaceConstraint* Rules::readActivitiesOccupyMaxDifferentRooms(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesOccupyMaxDifferentRooms");
	ConstraintActivitiesOccupyMaxDifferentRooms* cn=new ConstraintActivitiesOccupyMaxDifferentRooms();
	
	int ac=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";

		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			ac=text.toInt();
			xmlReadingLog+="    Read number of activities="+CustomFETString::number(ac)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesIds.append(text.toInt());
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesIds[cn->activitiesIds.count()-1])+"\n";
		}
		else if(xmlReader.name()=="Max_Number_of_Different_Rooms"){
			QString text=xmlReader.readElementText();
			cn->maxDifferentRooms=text.toInt();
			xmlReadingLog+="    Read max number of different rooms="+CustomFETString::number(cn->maxDifferentRooms)+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(!(ac==cn->activitiesIds.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(ac==cn->activitiesIds.count());
	
	return cn;
}

////////////////

//2013-09-14
SpaceConstraint* Rules::readActivitiesSameRoomIfConsecutive(QXmlStreamReader& xmlReader, FakeString& xmlReadingLog){
	assert(xmlReader.isStartElement() && xmlReader.name()=="ConstraintActivitiesSameRoomIfConsecutive");
	ConstraintActivitiesSameRoomIfConsecutive* cn=new ConstraintActivitiesSameRoomIfConsecutive();
	
	int ac=0;
	
	while(xmlReader.readNextStartElement()){
		xmlReadingLog+="    Found "+xmlReader.name().toString()+" tag\n";

		if(xmlReader.name()=="Weight_Percentage"){
			QString text=xmlReader.readElementText();
			cn->weightPercentage=customFETStrToDouble(text);
			xmlReadingLog+="    Adding weight percentage="+CustomFETString::number(cn->weightPercentage)+"\n";
		}
		else if(xmlReader.name()=="Active"){
			QString text=xmlReader.readElementText();
			if(text=="false"){
				cn->active=false;
			}
		}
		else if(xmlReader.name()=="Comments"){
			QString text=xmlReader.readElementText();
			cn->comments=text;
		}
		else if(xmlReader.name()=="Number_of_Activities"){
			QString text=xmlReader.readElementText();
			ac=text.toInt();
			xmlReadingLog+="    Read number of activities="+CustomFETString::number(ac)+"\n";
		}
		else if(xmlReader.name()=="Activity_Id"){
			QString text=xmlReader.readElementText();
			cn->activitiesIds.append(text.toInt());
			xmlReadingLog+="    Read activity id="+CustomFETString::number(cn->activitiesIds[cn->activitiesIds.count()-1])+"\n";
		}
		else{
			xmlReader.skipCurrentElement();
			xmlReaderNumberOfUnrecognizedFields++;
		}
	}
	
	if(!(ac==cn->activitiesIds.count())){
		xmlReader.raiseError(tr("%1 does not coincide with the number of read %2").arg("Number_of_Activities").arg("Activity_Id"));
		delete cn;
		cn=NULL;
		return NULL;
	}
	assert(ac==cn->activitiesIds.count());
	
	return cn;
}

////////////////
