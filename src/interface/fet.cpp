/*
File fet.cpp - this is where the program FET starts
*/

/***************************************************************************
                          fet.cpp  -  description
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

#include "fet.h"

#include "matrix.h"

#include "messageboxes.h"

#ifndef FET_COMMAND_LINE
#include <QMessageBox>

#include <QWidget>
#endif

#include <QLocale>
#include <QTime>
#include <QDate>
#include <QDateTime>

#include <QSet>

static QSet<QString> languagesSet;

#include <ctime>

#include "timetableexport.h"
#include "generate.h"

#include "timetable_defs.h"
#include "timetable.h"

#ifndef FET_COMMAND_LINE
#include "fetmainform.h"

#include "helpaboutform.h"
#include "helpfaqform.h"
#include "helptipsform.h"
#include "helpinstructionsform.h"

#include "timetableshowconflictsform.h"
#include "timetableviewstudentsform.h"
#include "timetableviewteachersform.h"
#include "timetableviewroomsform.h"
#endif

#include <QCoreApplication>

#ifndef FET_COMMAND_LINE
#include <QApplication>

#include <QSettings>
#include <QRect>
#endif

#include <QMutex>
#include <QString>
#include <QTranslator>

#include <QDir>

#include <QTextStream>
#include <QFile>

#include <csignal>

#include <iostream>
using namespace std;

#ifndef FET_COMMAND_LINE
extern QRect mainFormSettingsRect;
extern int MAIN_FORM_SHORTCUTS_TAB_POSITION;
#endif

extern Solution highestStageSolution;

extern int maxActivitiesPlaced;

#ifndef FET_COMMAND_LINE
extern int initialOrderOfActivitiesIndices[MAX_ACTIVITIES];
#else
int initialOrderOfActivitiesIndices[MAX_ACTIVITIES];
#endif

extern bool students_schedule_ready, teachers_schedule_ready, rooms_schedule_ready;

#ifndef FET_COMMAND_LINE
extern QMutex myMutex;
#else
QMutex myMutex;
#endif

void writeDefaultSimulationParameters();

QTranslator translator;

/**
The one and only instantiation of the main class.
*/
Timetable gt;

/**
The name of the file from where the rules are read.
*/
QString INPUT_FILENAME_XML;

/**
The working directory
*/
QString WORKING_DIRECTORY;

/**
The import directory
*/
QString IMPORT_DIRECTORY;

/*qint16 teachers_timetable_weekly[MAX_TEACHERS][MAX_DAYS_PER_WEEK][MAX_HOURS_PER_DAY];
qint16 students_timetable_weekly[MAX_TOTAL_SUBGROUPS][MAX_DAYS_PER_WEEK][MAX_HOURS_PER_DAY];
qint16 rooms_timetable_weekly[MAX_ROOMS][MAX_DAYS_PER_WEEK][MAX_HOURS_PER_DAY];*/
Matrix3D<int> teachers_timetable_weekly;
Matrix3D<int> students_timetable_weekly;
Matrix3D<int> rooms_timetable_weekly;
//QList<qint16> teachers_free_periods_timetable_weekly[TEACHERS_FREE_PERIODS_N_CATEGORIES][MAX_DAYS_PER_WEEK][MAX_HOURS_PER_DAY];
Matrix3D<QList<int> > teachers_free_periods_timetable_weekly;

#ifndef FET_COMMAND_LINE
QApplication* pqapplication=NULL;

FetMainForm* pFetMainForm=NULL;
#endif

extern int XX;
extern int YY;

Generate* terminateGeneratePointer;

//for command line version, if the user stops using a signal
#ifdef FET_COMMAND_LINE
void terminate(int param)
{
	Q_UNUSED(param);

	assert(terminateGeneratePointer!=NULL);
	
	terminateGeneratePointer->abortOptimization=true;
}

void usage(QTextStream* out, const QString& error)
{
	QString s="";
	
	s+=QString("Incorrect command-line parameters (%1).").arg(error);
	
	s+="\n\n";
	
	s+=QString(
		"Command line usage: \"fet-cl --inputfile=x [--outputdir=d] [--timelimitseconds=y] [--htmllevel=z] [--language=t] "
		"[--printactivitytags=a] [--printnotavailable=u] [--printbreak=b] [--dividetimeaxisbydays=v] [--duplicateverticalheaders=e] "
		"[--printsimultaneousactivities=w] [--randomseedx=rx --randomseedy=ry] [--warnifusingnotperfectconstraints=s] "
		"[--warnifusingstudentsminhoursdailywithallowemptydays=p] [--warnifusinggroupactivitiesininitialorder=g] [--verbose=r]\",\n"
		"where:\nx is the input file, for instance \"data.fet\"\n"
		"d is the path to results directory, without trailing slash or backslash (default is current working path). "
		"Make sure you have write permissions there.\n"
		"y is integer (seconds) (default 2000000000, which is practically infinite).\n"
		"z is integer from 0 to 6 and represents the detail level for the generated HTML timetables "
		"(default 2, larger values have more details/facilities and larger file sizes).\n"
		"t is one of en_US, ar, ca, da, de, el, es, fa, fr, gl, he, hu, id, it, lt, mk, ms, nl, pl, pt_BR, ro, ru, si, sk, sq, sr, tr, uk, uz, vi, zh_CN (default en_US).\n"
		"a is either true or false and represets if you want activity tags to be present in the final HTML timetables (default true).\n"
		"u is either true or false and represents if you want -x- (for true) or --- (for false) in the generated timetables for the "
		"not available slots (default true).\n"
		"b is either true or false and represents if you want -X- (for true) or --- (for false) in the generated timetables for the "
		"break slots (default true).\n"
		"v is either true or false, represents if you want the HTML timetables with time-axis divided by days (default false).\n"
		"e is either true or false, represents if you want the HTML timetables to duplicate vertical headers to the right of the tables, for easier reading (default false).\n"
		"w is either true or false, represents if you want the HTML timetables to show related activities which have constraints with same starting time (default false).\n"
		"(for instance, if A1 (T1, G1) and A2 (T2, G2) have constraint activities same starting time, then in T1's timetable will appear also A2, at the same slot "
		"as A1).\n"
		"rx is the random seed X component, minimum 1 to maximum 2147483646, ry is the random seed Y component, minimum 1 to maximum 2147483398"
		" (you can get the same timetable if the input file is identical, if the FET version is the same and if the random seed X and Y components are the same).\n"
		"s is either true or false, represents whether you want a message box to be shown, with a warning, if the input file contains not perfect constraints "
		"(activity tag max hours daily or students max gaps per day) (default true).\n"
		"p is either true or false, represents whether you want a message box to be shown, with a warning, if the input file contains nonstandard constraints "
		"students min hours daily with allow empty days (default true).\n"
		"g is either true or false, represents whether you want a message box to be shown, with a warning, if the input file contains nonstandard timetable "
		"generation options to group activities in the initial order (default true).\n"
		"r is either true or false, represents whether you want additional generation messages and other messages to be shown on the command line (default false)."
		"\n"
		"Alternatively, you can run \"fet-cl --version [--outputdir=d]\" to get the current FET version. "
		"where:\nd is the path to results directory, without trailing slash or backslash (default is current working path). "
		"Make sure you have write permissions there.\n"
		"(If you specify the --version argument, FET just prints version number on the command line prompt and in the output directory and exits.)\n"
		"\n"
		"You can ask the FET command line process to stop the timetable generation, by sending it the SIGTERM signal. "
		"FET will then write the current timetable and the highest stage timetable and exit."
	);
	
	cout<<qPrintable(s)<<endl;
	if(out!=NULL)
		(*out)<<qPrintable(s)<<endl;
}
#endif

#ifndef FET_COMMAND_LINE
void readSimulationParameters()
{
	const QString predefDir=QDir::homePath()+FILE_SEP+"fet-results";

	QSettings newSettings(COMPANY, PROGRAM);

	if(newSettings.contains("output-directory")){
		OUTPUT_DIR=newSettings.value("output-directory").toString();
		QDir dir;
		if(!dir.exists(OUTPUT_DIR)){
			bool t=dir.mkpath(OUTPUT_DIR);
			if(!t){
				QMessageBox::warning(NULL, FetTranslate::tr("FET warning"), FetTranslate::tr("Output directory %1 does not exist and cannot be"
				 " created - output directory will be made the default value %2")
				 .arg(QDir::toNativeSeparators(OUTPUT_DIR)).arg(QDir::toNativeSeparators(predefDir)));
				OUTPUT_DIR=predefDir;
			}
		}
	}
	else{
		OUTPUT_DIR=predefDir;
	}

#ifndef USE_SYSTEM_LOCALE
	FET_LANGUAGE=newSettings.value("language", "en_US").toString();
#else
	if(newSettings.contains("language")){
		FET_LANGUAGE=newSettings.value("language").toString();
	}
	else{
		FET_LANGUAGE=QLocale::system().name();

		bool ok=false;
		foreach(QString s, languagesSet){
			if(FET_LANGUAGE.left(s.length())==s){
				FET_LANGUAGE=s;
				ok=true;
				break;
			}
		}
		if(!ok)
			FET_LANGUAGE="en_US";
	}
#endif
	
	WORKING_DIRECTORY=newSettings.value("working-directory", "examples").toString();
	IMPORT_DIRECTORY=newSettings.value("import-directory", OUTPUT_DIR).toString();
	
	QDir d(WORKING_DIRECTORY);
	if(!d.exists())
		WORKING_DIRECTORY="examples";
	QDir d2(WORKING_DIRECTORY);
	if(!d2.exists())
		WORKING_DIRECTORY=QDir::homePath();
	else
		WORKING_DIRECTORY=d2.absolutePath();

	QDir i(IMPORT_DIRECTORY);
	if(!i.exists())
		IMPORT_DIRECTORY=OUTPUT_DIR;
	
	checkForUpdates=newSettings.value("check-for-updates", "false").toBool();

	QString ver=newSettings.value("version", "-1").toString();
	
	TIMETABLE_HTML_LEVEL=newSettings.value("html-level", "2").toInt();
	TIMETABLE_HTML_PRINT_ACTIVITY_TAGS=newSettings.value("print-activity-tags", "true").toBool();
	PRINT_ACTIVITIES_WITH_SAME_STARTING_TIME=newSettings.value("print-activities-with-same-starting-time", "false").toBool();
	PRINT_NOT_AVAILABLE_TIME_SLOTS=newSettings.value("print-not-available", "true").toBool();
	PRINT_BREAK_TIME_SLOTS=newSettings.value("print-break", "true").toBool();
	DIVIDE_HTML_TIMETABLES_WITH_TIME_AXIS_BY_DAYS=newSettings.value("divide-html-timetables-with-time-axis-by-days", "false").toBool();
	TIMETABLE_HTML_REPEAT_NAMES=newSettings.value("timetables-repeat-vertical-names", "false").toBool();
	
	USE_GUI_COLORS=newSettings.value("use-gui-colors", "false").toBool();

/////////confirmations
	CONFIRM_ACTIVITY_PLANNING=newSettings.value("confirm-activity-planning", "true").toBool();
	CONFIRM_SPREAD_ACTIVITIES=newSettings.value("confirm-spread-activities", "true").toBool();
	CONFIRM_REMOVE_REDUNDANT=newSettings.value("confirm-remove-redundant", "true").toBool();
	CONFIRM_SAVE_TIMETABLE=newSettings.value("confirm-save-data-and-timetable", "true").toBool();
/////////

	ENABLE_ACTIVITY_TAG_MAX_HOURS_DAILY=newSettings.value("enable-activity-tag-max-hours-daily", "false").toBool();
	ENABLE_STUDENTS_MAX_GAPS_PER_DAY=newSettings.value("enable-students-max-gaps-per-day", "false").toBool();
	SHOW_WARNING_FOR_NOT_PERFECT_CONSTRAINTS=newSettings.value("warn-if-using-not-perfect-constraints", "true").toBool();
	ENABLE_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS=newSettings.value("enable-students-min-hours-daily-with-allow-empty-days", "false").toBool();
	SHOW_WARNING_FOR_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS=newSettings.value("warn-if-using-students-min-hours-daily-with-allow-empty-days", "true").toBool();
	
	ENABLE_GROUP_ACTIVITIES_IN_INITIAL_ORDER=newSettings.value("enable-group-activities-in-initial-order", "false").toBool();
	SHOW_WARNING_FOR_GROUP_ACTIVITIES_IN_INITIAL_ORDER=newSettings.value("warn-if-using-group-activities-in-initial-order", "true").toBool();
	
	//main form
	QRect rect=newSettings.value("FetMainForm/geometry", QRect(0,0,0,0)).toRect();
	mainFormSettingsRect=rect;
	//MAIN_FORM_SHORTCUTS_TAB_POSITION=newSettings.value("FetMainForm/shortcuts-tab-position", "0").toInt();
	MAIN_FORM_SHORTCUTS_TAB_POSITION=0; //always restoring to the first page, as suggested by a user
	SHOW_SHORTCUTS_ON_MAIN_WINDOW=newSettings.value("FetMainForm/show-shortcuts", "true").toBool();
	
	if(VERBOSE){
		cout<<"Settings read"<<endl;
	}
}

void writeSimulationParameters()
{
	QSettings settings(COMPANY, PROGRAM);

	settings.setValue("output-directory", OUTPUT_DIR);
	settings.setValue("language", FET_LANGUAGE);
	settings.setValue("working-directory", WORKING_DIRECTORY);
	settings.setValue("import-directory", IMPORT_DIRECTORY);
	settings.setValue("version", FET_VERSION);
	settings.setValue("check-for-updates", checkForUpdates);
	settings.setValue("html-level", TIMETABLE_HTML_LEVEL);
	settings.setValue("print-activity-tags", TIMETABLE_HTML_PRINT_ACTIVITY_TAGS);
	settings.setValue("print-activities-with-same-starting-time", PRINT_ACTIVITIES_WITH_SAME_STARTING_TIME);
	settings.setValue("divide-html-timetables-with-time-axis-by-days", DIVIDE_HTML_TIMETABLES_WITH_TIME_AXIS_BY_DAYS);
	settings.setValue("timetables-repeat-vertical-names", TIMETABLE_HTML_REPEAT_NAMES);
	settings.setValue("print-not-available", PRINT_NOT_AVAILABLE_TIME_SLOTS);
	settings.setValue("print-break", PRINT_BREAK_TIME_SLOTS);
	
	settings.setValue("use-gui-colors", USE_GUI_COLORS);
	
///////////confirmations
	settings.setValue("confirm-activity-planning", CONFIRM_ACTIVITY_PLANNING);
	settings.setValue("confirm-spread-activities", CONFIRM_SPREAD_ACTIVITIES);
	settings.setValue("confirm-remove-redundant", CONFIRM_REMOVE_REDUNDANT);
	settings.setValue("confirm-save-data-and-timetable", CONFIRM_SAVE_TIMETABLE);
///////////

	settings.setValue("enable-activity-tag-max-hours-daily", ENABLE_ACTIVITY_TAG_MAX_HOURS_DAILY);
	settings.setValue("enable-students-max-gaps-per-day", ENABLE_STUDENTS_MAX_GAPS_PER_DAY);
	settings.setValue("warn-if-using-not-perfect-constraints", SHOW_WARNING_FOR_NOT_PERFECT_CONSTRAINTS);
	settings.setValue("enable-students-min-hours-daily-with-allow-empty-days", ENABLE_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS);
	settings.setValue("warn-if-using-students-min-hours-daily-with-allow-empty-days", SHOW_WARNING_FOR_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS);

	settings.setValue("enable-group-activities-in-initial-order", ENABLE_GROUP_ACTIVITIES_IN_INITIAL_ORDER);
	settings.setValue("warn-if-using-group-activities-in-initial-order", SHOW_WARNING_FOR_GROUP_ACTIVITIES_IN_INITIAL_ORDER);

	//main form
	settings.setValue("FetMainForm/geometry", mainFormSettingsRect);
	//settings.setValue("FetMainForm/shortcuts-tab-position", MAIN_FORM_SHORTCUTS_TAB_POSITION);
	//settings.setValue("FetMainForm/shortcuts-tab-position", 0); //always starting on the first page, as suggested by a user
	settings.setValue("FetMainForm/show-shortcuts", SHOW_SHORTCUTS_ON_MAIN_WINDOW);
}
#endif

void initLanguagesSet()
{
	//This is one of the two places to insert a new language in the sources (the other one is in fetmainform.cpp).
	languagesSet.clear();
	languagesSet.insert("en_US");
	languagesSet.insert("ar");
    languagesSet.insert("bg");
    languagesSet.insert("ca");
	languagesSet.insert("de");
	languagesSet.insert("el");
	languagesSet.insert("es");
	languagesSet.insert("fr");
	languagesSet.insert("hu");
	languagesSet.insert("id");
	languagesSet.insert("it");
	languagesSet.insert("lt");
	languagesSet.insert("mk");
	languagesSet.insert("ms");
	languagesSet.insert("nl");
	languagesSet.insert("pl");
	languagesSet.insert("ro");
	languagesSet.insert("tr");
	languagesSet.insert("ru");
	languagesSet.insert("fa");
	languagesSet.insert("uk");
	languagesSet.insert("pt_BR");
	languagesSet.insert("da");
	languagesSet.insert("si");
	languagesSet.insert("sk");
	languagesSet.insert("he");
	languagesSet.insert("sr");
	languagesSet.insert("gl");
	languagesSet.insert("vi");
	languagesSet.insert("uz");
	languagesSet.insert("sq");
	languagesSet.insert("zh_CN");
}

#ifndef FET_COMMAND_LINE
void setLanguage(QApplication& qapplication, QWidget* parent)
#else
void setLanguage(QCoreApplication& qapplication, QWidget* parent)
#endif
{
	Q_UNUSED(qapplication); //silence MSVC wrong warning

	static int cntTranslators=0;
	
	if(cntTranslators>0){
		qapplication.removeTranslator(&translator);
		cntTranslators=0;
	}

	//translator stuff
	QDir d("/usr/share/fet/translations");
	
	bool translation_loaded=false;
	
	if(FET_LANGUAGE!="en_US" && languagesSet.contains(FET_LANGUAGE)){
		translation_loaded=translator.load("fet_"+FET_LANGUAGE, qapplication.applicationDirPath());
		if(!translation_loaded){
			translation_loaded=translator.load("fet_"+FET_LANGUAGE, qapplication.applicationDirPath()+"/translations");
			if(!translation_loaded){
				if(d.exists()){
					translation_loaded=translator.load("fet_"+FET_LANGUAGE, "/usr/share/fet/translations");
				}
			}
		}
	}
	else{
		if(FET_LANGUAGE!="en_US"){
			FetMessage::warning(parent, QString("FET warning"),
			 QString("Specified language is incorrect - making it en_US (US English)"));
			FET_LANGUAGE="en_US";
		}
		
		assert(FET_LANGUAGE=="en_US");
		
		translation_loaded=true;
	}
	
	if(!translation_loaded){
		FetMessage::warning(parent, QString("FET warning"),
		 QString("Translation for specified language not loaded - maybe the translation file is missing - setting the language to en_US (US English)")
		 +"\n\n"+
		 QString("FET searched for the translation file %1 in the directory %2, then in the directory %3 and "
		 "then in the directory %4 (under systems that support such a directory), but could not find it.")
		 .arg("fet_"+FET_LANGUAGE+".qm")
		 .arg(QDir::toNativeSeparators(qapplication.applicationDirPath()))
		 .arg(QDir::toNativeSeparators(qapplication.applicationDirPath()+"/translations"))
		 .arg("/usr/share/fet/translations")
		 );
		FET_LANGUAGE="en_US";
	}
	
	if(FET_LANGUAGE=="ar" || FET_LANGUAGE=="he" || FET_LANGUAGE=="fa" || FET_LANGUAGE=="ur" /* and others? */){
		LANGUAGE_STYLE_RIGHT_TO_LEFT=true;
	}
	else{
		LANGUAGE_STYLE_RIGHT_TO_LEFT=false;
	}
	
	if(FET_LANGUAGE=="zh_CN"){
		LANGUAGE_FOR_HTML="zh-Hans";
	}
	else if(FET_LANGUAGE=="zh_TW"){
		LANGUAGE_FOR_HTML="zh-Hant";
	}
	else if(FET_LANGUAGE=="en_US"){
		LANGUAGE_FOR_HTML=FET_LANGUAGE.left(2);
	}
	else{
		LANGUAGE_FOR_HTML=FET_LANGUAGE;
		LANGUAGE_FOR_HTML.replace(QString("_"), QString("-"));
	}
	
	assert(cntTranslators==0);
	if(FET_LANGUAGE!="en_US"){
		qapplication.installTranslator(&translator);
		cntTranslators=1;
	}
	
#ifndef FET_COMMAND_LINE
	if(LANGUAGE_STYLE_RIGHT_TO_LEFT==true)
		qapplication.setLayoutDirection(Qt::RightToLeft);
	
	//retranslate
	QList<QWidget*> tlwl=qapplication.topLevelWidgets();

	foreach(QWidget* wi, tlwl)
		if(wi->isVisible()){
			FetMainForm* mainform=qobject_cast<FetMainForm*>(wi);
			if(mainform!=NULL){
				mainform->retranslateUi(mainform);
				continue;
			}

			//help
			HelpAboutForm* aboutf=qobject_cast<HelpAboutForm*>(wi);
			if(aboutf!=NULL){
				aboutf->retranslateUi(aboutf);
				continue;
			}

			HelpFaqForm* faqf=qobject_cast<HelpFaqForm*>(wi);
			if(faqf!=NULL){
				faqf->retranslateUi(faqf);
				faqf->setText();
				continue;
			}

			HelpTipsForm* tipsf=qobject_cast<HelpTipsForm*>(wi);
			if(tipsf!=NULL){
				tipsf->retranslateUi(tipsf);
				tipsf->setText();
				continue;
			}

			HelpInstructionsForm* instrf=qobject_cast<HelpInstructionsForm*>(wi);
			if(instrf!=NULL){
				instrf->retranslateUi(instrf);
				instrf->setText();
				continue;
			}
			//////
			
			//timetable
			TimetableViewStudentsForm* vsf=qobject_cast<TimetableViewStudentsForm*>(wi);
			if(vsf!=NULL){
				vsf->retranslateUi(vsf);
				vsf->updateStudentsTimetableTable();
				continue;
			}

			TimetableViewTeachersForm* vtchf=qobject_cast<TimetableViewTeachersForm*>(wi);
			if(vtchf!=NULL){
				vtchf->retranslateUi(vtchf);
				vtchf->updateTeachersTimetableTable();
				continue;
			}

			TimetableViewRoomsForm* vrf=qobject_cast<TimetableViewRoomsForm*>(wi);
			if(vrf!=NULL){
				vrf->retranslateUi(vrf);
				vrf->updateRoomsTimetableTable();
				continue;
			}

			TimetableShowConflictsForm* scf=qobject_cast<TimetableShowConflictsForm*>(wi);
			if(scf!=NULL){
				scf->retranslateUi(scf);
				continue;
			}
		}
#endif
}

void SomeQtTranslations()
{
	//This function is never actually used
	//It just contains some commonly used Qt strings, so that some Qt strings of FET are translated.
	QString s1=QCoreApplication::translate("QDialogButtonBox", "&OK", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different");
	Q_UNUSED(s1);
	QString s2=QCoreApplication::translate("QDialogButtonBox", "OK");
	Q_UNUSED(s2);
	
	QString s3=QCoreApplication::translate("QDialogButtonBox", "&Cancel", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different");
	Q_UNUSED(s3);
	QString s4=QCoreApplication::translate("QDialogButtonBox", "Cancel");
	Q_UNUSED(s4);
	
	QString s5=QCoreApplication::translate("QDialogButtonBox", "&Yes", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different");
	Q_UNUSED(s5);
	QString s6=QCoreApplication::translate("QDialogButtonBox", "Yes to &All", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different. Please keep the translation short.");
	Q_UNUSED(s6);
	QString s7=QCoreApplication::translate("QDialogButtonBox", "&No", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different");
	Q_UNUSED(s7);
	QString s8=QCoreApplication::translate("QDialogButtonBox", "N&o to All", "Accelerator key (letter after ampersand) for &OK, &Cancel, &Yes, Yes to &All, &No, N&o to All, must be different. Please keep the translation short.");
	Q_UNUSED(s8);
}

/**
FET starts here
*/
int main(int argc, char **argv)
{
#ifndef FET_COMMAND_LINE
	QApplication qapplication(argc, argv);
#else
	QCoreApplication qCoreApplication(argc, argv);
#endif

	initLanguagesSet();

	VERBOSE=false;

	terminateGeneratePointer=NULL;
	
	students_schedule_ready=0;
	teachers_schedule_ready=0;
	rooms_schedule_ready=0;

#ifndef FET_COMMAND_LINE
	QObject::connect(&qapplication, SIGNAL(lastWindowClosed()), &qapplication, SLOT(quit()));
#endif

	srand(unsigned(time(NULL))); //useless, I use randomKnuth(), but just in case I use somewhere rand() by mistake...

	initRandomKnuth();

	OUTPUT_DIR=QDir::homePath()+FILE_SEP+"fet-results";
	
	QStringList _args=QCoreApplication::arguments();

#ifndef FET_COMMAND_LINE
	if(_args.count()==1){
		readSimulationParameters();
	
		QDir dir;
	
		bool t=true;

		//make sure that the output directory exists
		if(!dir.exists(OUTPUT_DIR))
			t=dir.mkpath(OUTPUT_DIR);

		if(!t){
			QMessageBox::critical(NULL, FetTranslate::tr("FET critical"), FetTranslate::tr("Cannot create or use %1 directory (where the results should be stored) - you can continue operation, but you might not be able to work with FET."
			 " Maybe you can try to change the output directory from the 'Settings' menu. If this is a bug - please report it.").arg(QDir::toNativeSeparators(OUTPUT_DIR)));
		}
		
		QString testFileName=OUTPUT_DIR+FILE_SEP+"test_write_permissions_1.tmp";
		QFile test(testFileName);
		bool existedBefore=test.exists();
		bool t_t=test.open(QIODevice::ReadWrite);
		if(!t_t){
			QMessageBox::critical(NULL, FetTranslate::tr("FET critical"), FetTranslate::tr("You don't have write permissions in the output directory "
			 "(FET cannot open or create file %1) - you might not be able to work correctly with FET. Maybe you can try to change the output directory from the 'Settings' menu."
			 " If this is a bug - please report it.").arg(testFileName));
		}
		else{
			test.close();
			if(!existedBefore)
				test.remove();
		}

		setLanguage(qapplication, NULL);

		pqapplication=&qapplication;
		FetMainForm fetMainForm;
		pFetMainForm=&fetMainForm;
		fetMainForm.show();

		int tmp2=qapplication.exec();
	
		writeSimulationParameters();
	
		if(VERBOSE){
			cout<<"Settings saved"<<endl;
		}
	
		pFetMainForm=NULL;
	
		return tmp2;
	}
	else{
		QMessageBox::warning(NULL, FetTranslate::tr("FET warning"), FetTranslate::tr("To start FET in interface mode, please do"
		 " not give any command-line parameters to the FET executable"));
		
		return 1;
	}
#else
	/////////////////////////////////////////////////
	//begin command line
	if(_args.count()>1){
		int randomSeedX=-1;
		int randomSeedY=-1;
		bool randomSeedXSpecified=false;
		bool randomSeedYSpecified=false;
	
		QString outputDirectory="";
	
		INPUT_FILENAME_XML="";
		
		QString filename="";
		
		int secondsLimit=2000000000;
		
		TIMETABLE_HTML_LEVEL=2;
		
		TIMETABLE_HTML_PRINT_ACTIVITY_TAGS=true;
		
		FET_LANGUAGE="en_US";
		
		PRINT_NOT_AVAILABLE_TIME_SLOTS=true;
		
		PRINT_BREAK_TIME_SLOTS=true;
		
		DIVIDE_HTML_TIMETABLES_WITH_TIME_AXIS_BY_DAYS=false;
		
		TIMETABLE_HTML_REPEAT_NAMES=false;

		PRINT_ACTIVITIES_WITH_SAME_STARTING_TIME=false;
		
		QStringList unrecognizedOptions;
		
		SHOW_WARNING_FOR_NOT_PERFECT_CONSTRAINTS=true;
		
		SHOW_WARNING_FOR_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS=true;
		
		SHOW_WARNING_FOR_GROUP_ACTIVITIES_IN_INITIAL_ORDER=true;
		
		bool showVersion=false;
		
		for(int i=1; i<_args.count(); i++){
			QString s=_args[i];
			
			if(s.left(12)=="--inputfile=")
				filename=QDir::fromNativeSeparators(s.right(s.length()-12));
			else if(s.left(19)=="--timelimitseconds=")
				secondsLimit=s.right(s.length()-19).toInt();
			else if(s.left(21)=="--timetablehtmllevel=")
				TIMETABLE_HTML_LEVEL=s.right(s.length()-21).toInt();
			else if(s.left(12)=="--htmllevel=")
				TIMETABLE_HTML_LEVEL=s.right(s.length()-12).toInt();
			else if(s.left(20)=="--printactivitytags="){
				if(s.right(5)=="false")
					TIMETABLE_HTML_PRINT_ACTIVITY_TAGS=false;
			}
			else if(s.left(11)=="--language=")
				FET_LANGUAGE=s.right(s.length()-11);
			else if(s.left(20)=="--printnotavailable="){
				if(s.right(5)=="false")
					PRINT_NOT_AVAILABLE_TIME_SLOTS=false;
			}
			else if(s.left(13)=="--printbreak="){
				if(s.right(5)=="false")
					PRINT_BREAK_TIME_SLOTS=false;
			}
			else if(s.left(23)=="--dividetimeaxisbydays="){
				if(s.right(4)=="true")
					DIVIDE_HTML_TIMETABLES_WITH_TIME_AXIS_BY_DAYS=true;
			}
			else if(s.left(27)=="--duplicateverticalheaders="){
				if(s.right(4)=="true")
					TIMETABLE_HTML_REPEAT_NAMES=true;
			}
			else if(s.left(12)=="--outputdir="){
				outputDirectory=QDir::fromNativeSeparators(s.right(s.length()-12));
			}
			else if(s.left(30)=="--printsimultaneousactivities="){
				if(s.right(4)=="true")
					PRINT_ACTIVITIES_WITH_SAME_STARTING_TIME=true;
			}
			else if(s.left(14)=="--randomseedx="){
				randomSeedXSpecified=true;
				randomSeedX=s.right(s.length()-14).toInt();
			}
			else if(s.left(14)=="--randomseedy="){
				randomSeedYSpecified=true;
				randomSeedY=s.right(s.length()-14).toInt();
			}
			else if(s.left(35)=="--warnifusingnotperfectconstraints="){
				if(s.right(5)=="false")
					SHOW_WARNING_FOR_NOT_PERFECT_CONSTRAINTS=false;
			}
			else if(s.left(53)=="--warnifusingstudentsminhoursdailywithallowemptydays="){
				if(s.right(5)=="false")
					SHOW_WARNING_FOR_STUDENTS_MIN_HOURS_DAILY_WITH_ALLOW_EMPTY_DAYS=false;
			}
			else if(s.left(43)=="--warnifusinggroupactivitiesininitialorder="){
				if(s.right(5)=="false")
					SHOW_WARNING_FOR_GROUP_ACTIVITIES_IN_INITIAL_ORDER=false;
			}
			else if(s.left(10)=="--verbose="){
				if(s.right(4)=="true")
					VERBOSE=true;
			}
			else if(s=="--version"){
				showVersion=true;
			}
			else
				unrecognizedOptions.append(s);
		}
		
		INPUT_FILENAME_XML=filename;
		
		QString initialDir=outputDirectory;
		if(initialDir!="")
			initialDir.append(FILE_SEP);
		
		if(outputDirectory!="")
			outputDirectory.append(FILE_SEP);
		outputDirectory.append("timetables");

		//////////
		if(INPUT_FILENAME_XML!=""){
			outputDirectory.append(FILE_SEP);
			outputDirectory.append(INPUT_FILENAME_XML.right(INPUT_FILENAME_XML.length()-INPUT_FILENAME_XML.lastIndexOf(FILE_SEP)-1));
			if(outputDirectory.right(4)==".fet")
				outputDirectory=outputDirectory.left(outputDirectory.length()-4);
		}
		//////////
		
		QDir dir;
		QString logsDir=initialDir+"logs";
		if(!dir.exists(logsDir))
			dir.mkpath(logsDir);
		logsDir.append(FILE_SEP);
		
		////////
		QFile logFile(logsDir+"result.txt");
		bool tttt=logFile.open(QIODevice::WriteOnly);
		if(!tttt){
			cout<<"FET critical - you don't have write permissions in the output directory - (FET cannot open or create file "<<qPrintable(logsDir)<<"result.txt)."
			 " If this is a bug - please report it."<<endl;
			return 1;
		}
		QTextStream out(&logFile);
		///////
		
		setLanguage(qCoreApplication, NULL);
		
		if(showVersion){
			out<<"This file contains the result (log) of last operation"<<endl<<endl;
		
			QDate dat=QDate::currentDate();
			QTime tim=QTime::currentTime();
			QLocale loc(FET_LANGUAGE);
			QString sTime=loc.toString(dat, QLocale::ShortFormat)+" "+loc.toString(tim, QLocale::ShortFormat);
			out<<"FET command line request for version started on "<<qPrintable(sTime)<<endl<<endl;
	
			//QString qv=qVersion();
			out<<"FET version "<<qPrintable(FET_VERSION)<<endl;
			out<<"Free timetabling software, licensed under GNU GPL v2 or later"<<endl;
			out<<"Copyright (C) 2002-2014 Liviu Lalescu, Volker Dirr"<<endl;
			out<<"Homepage: http://lalescu.ro/liviu/fet/"<<endl;
			//out<<" (Using Qt version "<<qPrintable(qv)<<")"<<endl;
			cout<<"FET version "<<qPrintable(FET_VERSION)<<endl;
			cout<<"Free timetabling software, licensed under GNU GPL v2 or later"<<endl;
			cout<<"Copyright (C) 2002-2014 Liviu Lalescu, Volker Dirr"<<endl;
			cout<<"Homepage: http://lalescu.ro/liviu/fet/"<<endl;
			//cout<<" (Using Qt version "<<qPrintable(qv)<<")"<<endl;

			if(unrecognizedOptions.count()>0){
				out<<endl;
				cout<<endl;
				foreach(QString s, unrecognizedOptions){
					cout<<"Unrecognized option: "<<qPrintable(s)<<endl;
					out<<"Unrecognized option: "<<qPrintable(s)<<endl;
				}
			}

			logFile.close();
			return 0;
		}
		
		QFile maxPlacedActivityFile(logsDir+"max_placed_activities.txt");
		maxPlacedActivityFile.open(QIODevice::WriteOnly);
		QTextStream maxPlacedActivityStream(&maxPlacedActivityFile);
		maxPlacedActivityStream.setCodec("UTF-8");
		maxPlacedActivityStream.setGenerateByteOrderMark(true);
		maxPlacedActivityStream<<FetTranslate::tr("This is the list of max placed activities, chronologically. If FET could reach maximum n-th activity, look at the n+1-st activity"
			" in the initial order of the activities")<<endl<<endl;
				
		QFile initialOrderFile(logsDir+"initial_order.txt");
		initialOrderFile.open(QIODevice::WriteOnly);
		QTextStream initialOrderStream(&initialOrderFile);
		initialOrderStream.setCodec("UTF-8");
		initialOrderStream.setGenerateByteOrderMark(true);
						
		out<<"This file contains the result (log) of last operation"<<endl<<endl;
		
		QDate dat=QDate::currentDate();
		QTime tim=QTime::currentTime();
		QLocale loc(FET_LANGUAGE);
		QString sTime=loc.toString(dat, QLocale::ShortFormat)+" "+loc.toString(tim, QLocale::ShortFormat);
		out<<"FET command line simulation started on "<<qPrintable(sTime)<<endl<<endl;
		
		if(unrecognizedOptions.count()>0){
			foreach(QString s, unrecognizedOptions){
				cout<<"Unrecognized option: "<<qPrintable(s)<<endl;
				out<<"Unrecognized option: "<<qPrintable(s)<<endl;
			}
			cout<<endl;
			out<<endl;
		}
		
		if(outputDirectory!="")
			if(!dir.exists(outputDirectory))
				dir.mkpath(outputDirectory);
		
		if(outputDirectory!="")
			outputDirectory.append(FILE_SEP);
			
		QFile test(outputDirectory+"test_write_permissions_2.tmp");
		bool existedBefore=test.exists();
		bool t_t=test.open(QIODevice::ReadWrite);
		if(!t_t){
			cout<<"fet: critical error - you don't have write permissions in the output directory - (FET cannot open or create file "<<qPrintable(outputDirectory)<<"test_write_permissions_2.tmp)."
			 " If this is a bug - please report it."<<endl;
			out<<"fet: critical error - you don't have write permissions in the output directory - (FET cannot open or create file "<<qPrintable(outputDirectory)<<"test_write_permissions_2.tmp)."
			 " If this is a bug - please report it."<<endl;
			return 1;
		}
		else{
			test.close();
			if(!existedBefore)
				test.remove();
		}

		if(filename==""){
			usage(&out, QString("Input file not specified"));
			logFile.close();
			return 1;
		}
		if(secondsLimit==0){
			usage(&out, QString("Time limit is 0 seconds"));
			logFile.close();
			return 1;
		}
		if(TIMETABLE_HTML_LEVEL>6 || TIMETABLE_HTML_LEVEL<0){
			usage(&out, QString("Html level must be 0, 1, 2, 3, 4, 5 or 6"));
			logFile.close();
			return 1;
		}
		if(randomSeedXSpecified != randomSeedYSpecified){
			if(randomSeedXSpecified){
				usage(&out, QString("If you want to specify the random seed, you need to specify both the X and the Y components, not only the X component"));
			}
			else{
				assert(randomSeedYSpecified);
				usage(&out, QString("If you want to specify the random seed, you need to specify both the X and the Y components, not only the Y component"));
			}
			logFile.close();
			return 1;
		}
		assert(randomSeedXSpecified==randomSeedYSpecified);
		if(randomSeedXSpecified){
			if(randomSeedX<=0 || randomSeedX>=MM){
				usage(&out, QString("Random seed X component must be at least 1 and at most %1").arg(MM-1));
				logFile.close();
				return 1;
			}
		}
		if(randomSeedYSpecified){
			if(randomSeedY<=0 || randomSeedY>=MMM){
				usage(&out, QString("Random seed Y component must be at least 1 and at most %1").arg(MMM-1));
				logFile.close();
				return 1;
			}
		}
		
		if(randomSeedXSpecified){
			assert(randomSeedYSpecified);
			if(randomSeedX>0 && randomSeedX<MM && randomSeedY>0 && randomSeedY<MMM){
				XX=randomSeedX;
				YY=randomSeedY;
			}
		}
		
		if(TIMETABLE_HTML_LEVEL>6 || TIMETABLE_HTML_LEVEL<0)
			TIMETABLE_HTML_LEVEL=2;
	
		bool t=gt.rules.read(NULL, filename, true, initialDir);
		if(!t){
			cout<<"fet: cannot read input file (not existing or in use) - aborting"<<endl;
			out<<"Cannot read input file (not existing or in use) - aborting"<<endl;
			logFile.close();
			return 1;
		}
		
		t=gt.rules.computeInternalStructure(NULL);
		if(!t){
			cout<<"Cannot compute internal structure - aborting"<<endl;
			out<<"Cannot compute internal structure - aborting"<<endl;
			logFile.close();
			return 1;
		}
	
		Generate gen;

		terminateGeneratePointer=&gen;
		signal(SIGTERM, terminate);
	
		gen.abortOptimization=false;
		bool ok=gen.precompute(NULL, &initialOrderStream);
		
		initialOrderFile.close();
		
		if(!ok){
			cout<<"Cannot precompute - data is wrong - aborting"<<endl;
			out<<"Cannot precompute - data is wrong - aborting"<<endl;
			logFile.close();
			return 1;
		}
	
		bool impossible, timeExceeded;
		
		cout<<"Starting timetable generation..."<<endl;
		out<<"Starting timetable generation..."<<endl;
		if(VERBOSE){
			cout<<"secondsLimit=="<<secondsLimit<<endl;
		}
		//out<<"secondsLimit=="<<secondsLimit<<endl;
				
		TimetableExport::writeRandomSeedCommandLine(NULL, outputDirectory, true); //true represents 'before' state

		gen.generate(secondsLimit, impossible, timeExceeded, false, &maxPlacedActivityStream); //false means no thread
		
		maxPlacedActivityFile.close();
	
		if(impossible){
			cout<<"Impossible"<<endl;
			out<<"Impossible"<<endl;
		}
		//2012-01-24 - suggestion and code by Ian Holden (ian@ianholden.com), to write best and current timetable on time exceeded
		//previously, FET saved best and current timetable only on receiving SIGTERM
		//by Ian Holden (begin)
		else if(timeExceeded || gen.abortOptimization){
			if(timeExceeded){
				cout<<"Time exceeded"<<endl;
				out<<"Time exceeded"<<endl;
			}
			else if(gen.abortOptimization){
				cout<<"Simulation stopped"<<endl;
				out<<"Simulation stopped"<<endl;
			}
			//by Ian Holden (end)
			
			//2011-11-11 (1)
			//write current stage timetable
			Solution& cc=gen.c;

			//needed to find the conflicts strings
			QString tmp;
			cc.fitness(gt.rules, &tmp);

			TimetableExport::getStudentsTimetable(cc);
			TimetableExport::getTeachersTimetable(cc);
			TimetableExport::getRoomsTimetable(cc);

			QString toc=outputDirectory;
			if(toc!="" && toc.count()>=1 && toc.endsWith(FILE_SEP)){
				toc.chop(1);
				toc+=QString("-current"+FILE_SEP);
			}
			else if(toc==""){
				toc=QString("current"+FILE_SEP);
			}
			
			if(toc!="")
				if(!dir.exists(toc))
					dir.mkpath(toc);

			TimetableExport::writeSimulationResultsCommandLine(NULL, toc);
			
			QString s;

			if(maxActivitiesPlaced>=0 && maxActivitiesPlaced<gt.rules.nInternalActivities 
			 && initialOrderOfActivitiesIndices[maxActivitiesPlaced]>=0 && initialOrderOfActivitiesIndices[maxActivitiesPlaced]<gt.rules.nInternalActivities){
				s=FetTranslate::tr("FET managed to schedule correctly the first %1 most difficult activities."
				 " You can see initial order of placing the activities in the corresponding output file. The activity which might cause problems"
				 " might be the next activity in the initial order of evaluation. This activity is listed below:").arg(maxActivitiesPlaced);
				s+=QString("\n\n");
			
				int ai=initialOrderOfActivitiesIndices[maxActivitiesPlaced];

				s+=FetTranslate::tr("Id: %1 (%2)", "%1 is id of activity, %2 is detailed description of activity")
				 .arg(gt.rules.internalActivitiesList[ai].id)
				 .arg(getActivityDetailedDescription(gt.rules, gt.rules.internalActivitiesList[ai].id));
			}
			else
				s=FetTranslate::tr("Difficult activity cannot be computed - please report possible bug");
			
			s+=QString("\n\n----------\n\n");
			
			s+=FetTranslate::tr("Here are the placed activities which lead to an inconsistency, "
			 "in order from the first one to the last (the last one FET failed to schedule "
			 "and the last ones are most likely impossible):");
			s+="\n\n";
			for(int i=0; i<gen.nDifficultActivities; i++){
				int ai=gen.difficultActivities[i];

				s+=FetTranslate::tr("No: %1").arg(i+1);
		
				s+=", ";

				s+=FetTranslate::tr("Id: %1 (%2)", "%1 is id of activity, %2 is detailed description of activity")
					.arg(gt.rules.internalActivitiesList[ai].id)
					.arg(getActivityDetailedDescription(gt.rules, gt.rules.internalActivitiesList[ai].id));

				s+="\n";
			}
			
			QFile difficultActivitiesFile(logsDir+"difficult_activities.txt");
			bool t=difficultActivitiesFile.open(QIODevice::WriteOnly);
			if(!t){
				cout<<"FET critical - you don't have write permissions in the output directory - (FET cannot open or create file "<<qPrintable(logsDir)<<"difficult_activities.txt)."
				 " If this is a bug - please report it."<<endl;
				return 1;
			}
			QTextStream difficultActivitiesOut(&difficultActivitiesFile);
			difficultActivitiesOut.setCodec("UTF-8");
			difficultActivitiesOut.setGenerateByteOrderMark(true);
			
			difficultActivitiesOut<<s<<endl;
			
			//2011-11-11 (2)
			//write highest stage timetable
			Solution& ch=highestStageSolution;

			//needed to find the conflicts strings
			QString tmp2;
			ch.fitness(gt.rules, &tmp2);

			TimetableExport::getStudentsTimetable(ch);
			TimetableExport::getTeachersTimetable(ch);
			TimetableExport::getRoomsTimetable(ch);

			QString toh=outputDirectory;
			if(toh!="" && toh.count()>=1 && toh.endsWith(FILE_SEP)){
				toh.chop(1);
				toh+=QString("-highest"+FILE_SEP);
			}
			else if(toh==""){
				toh=QString("highest"+FILE_SEP);
			}
			
			if(toh!="")
				if(!dir.exists(toh))
					dir.mkpath(toh);

			TimetableExport::writeSimulationResultsCommandLine(NULL, toh);
		}
		else{
			cout<<"Simulation successful"<<endl;
			out<<"Simulation successful"<<endl;
		
			TimetableExport::writeRandomSeedCommandLine(NULL, outputDirectory, false); //false represents 'before' state

			Solution& c=gen.c;

			//needed to find the conflicts strings
			QString tmp;
			c.fitness(gt.rules, &tmp);
			
			TimetableExport::getStudentsTimetable(c);
			TimetableExport::getTeachersTimetable(c);
			TimetableExport::getRoomsTimetable(c);

			TimetableExport::writeSimulationResultsCommandLine(NULL, outputDirectory);
		}
	
		logFile.close();
		return 0;
	}
	else{
		usage(NULL, QString("No arguments given"));
		return 1;
	}
	//end command line
#endif
	/////////////////////////////////////////////////
}
