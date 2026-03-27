#include "Defines.h"

const char BulldozerStr[] PROGMEM = "Bulldozer";
const char RoadStr[] PROGMEM = "Road";
const char PowerlineStr[] PROGMEM = "Powerline";
const char ResidentialStr[] PROGMEM = "Residential";
const char CommericalStr[] PROGMEM = "Commercial";
const char IndustrialStr[] PROGMEM = "Industrial";
const char PowerplantStr[] PROGMEM = "Powerplant";
const char ParkStr[] PROGMEM = "Park";
const char PoliceDeptStr[] PROGMEM = "Police Dept";
const char FireDeptStr[] PROGMEM = "Fire Dept";
const char StadiumStr[] PROGMEM = "Stadium";
const char SaveLoadStr[] PROGMEM = "Save/Load";
const char BudgetStr[] PROGMEM = "Budget";

const char* const ToolbarStrings[] PROGMEM =
{
	BulldozerStr,
	RoadStr,
	PowerlineStr,
	ResidentialStr,
	CommericalStr,
	IndustrialStr,
	PowerplantStr,
	ParkStr,
	PoliceDeptStr,
	FireDeptStr,
	StadiumStr,
	SaveLoadStr,
	BudgetStr,
};

const char* GetToolbarString(int index)
{
	return (const char*)pgm_read_ptr(&ToolbarStrings[index]);
}

const char JanStr[] PROGMEM = "Jan";
const char FebStr[] PROGMEM = "Feb";
const char MarStr[] PROGMEM = "Mar";
const char AprStr[] PROGMEM = "Apr";
const char MayStr[] PROGMEM = "May";
const char JunStr[] PROGMEM = "Jun";
const char JulStr[] PROGMEM = "Jul";
const char AugStr[] PROGMEM = "Aug";
const char SepStr[] PROGMEM = "Sep";
const char OctStr[] PROGMEM = "Oct";
const char NovStr[] PROGMEM = "Nov";
const char DecStr[] PROGMEM = "Dec";

const char* const MonthStrings[] PROGMEM =
{
	JanStr,
	FebStr,
	MarStr,
	AprStr,
	MayStr,
	JunStr,
	JulStr,
	AugStr,
	SepStr,
	OctStr,
	NovStr,
	DecStr
};

const char* GetMonthString(int index)
{
	return (const char*)pgm_read_ptr(&MonthStrings[index]);
}


