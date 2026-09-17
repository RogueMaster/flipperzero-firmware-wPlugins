#include "Defines.h"

const char BulldozerStr[] = "Bulldozer";
const char RoadStr[] = "Road";
const char PowerlineStr[] = "Powerline";
const char ResidentialStr[] = "Residential";
const char CommericalStr[] = "Commercial";
const char IndustrialStr[] = "Industrial";
const char PowerplantStr[] = "Powerplant";
const char ParkStr[] = "Park";
const char PoliceDeptStr[] = "Police Dept";
const char FireDeptStr[] = "Fire Dept";
const char StadiumStr[] = "Stadium";
const char SaveLoadStr[] = "Save/Load";
const char BudgetStr[] = "Budget";

const char* const ToolbarStrings[] = {
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

const char* GetToolbarString(int index) {
    return ToolbarStrings[index];
}

const char JanStr[] = "Jan";
const char FebStr[] = "Feb";
const char MarStr[] = "Mar";
const char AprStr[] = "Apr";
const char MayStr[] = "May";
const char JunStr[] = "Jun";
const char JulStr[] = "Jul";
const char AugStr[] = "Aug";
const char SepStr[] = "Sep";
const char OctStr[] = "Oct";
const char NovStr[] = "Nov";
const char DecStr[] = "Dec";

const char* const MonthStrings[] =
    {JanStr, FebStr, MarStr, AprStr, MayStr, JunStr, JulStr, AugStr, SepStr, OctStr, NovStr, DecStr};

const char* GetMonthString(int index) {
    return MonthStrings[index];
}
