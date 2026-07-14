#include "tunings.h"

const TUNING Guitar6Tunings[] = {
    Guitar6Standard,
    Guitar6DropD,
    Guitar6DADGAD,
    Guitar6D,
    Guitar6CGCFGCSharp,
    Guitar6DropC,
    Guitar6CGCFGC};

const TUNING Guitar7Tunings[] = {Guitar7Standard, Guitar7DropA, Guitar7A};
const VARIATION GuitarVariations[] = {
    {"6 strings", (TUNING*)Guitar6Tunings, 7},
    {"7 strings", (TUNING*)Guitar7Tunings, 3}};

const TUNING Bass4Tunings[] =
    {Bass4Standard, Bass4Tenor, Bass4DropD, Bass4D, Bass4DropCSharp, Bass4DropC};
const TUNING Bass5Tunings[] = {Bass5Standard, Bass5Tenor, Bass5DropA};
const VARIATION BassVariations[] = {
    {"4 strings", (TUNING*)Bass4Tunings, 6},
    {"5 strings", (TUNING*)Bass5Tunings, 3}};

const TUNING Ukulele4Tunings[] = {Ukulele4Standard};
const VARIATION UkuleleVariations[] = {{"4 strings", (TUNING*)Ukulele4Tunings, 1}};

const TUNING Banjo5Tunings[] = {Banjo5Standard};
const VARIATION BanjoVariations[] = {{"5 strings", (TUNING*)Banjo5Tunings, 1}};

const TUNING CigarBox3Tunings[] = {CigarBox3OpenG, CigarBox3OpenD, CigarBox3OpenA};
const TUNING CigarBox4Tunings[] = {CigarBox4OpenG};
const VARIATION CigarBoxVariations[] = {
    {"3 strings", (TUNING*)CigarBox3Tunings, 3},
    {"4 strings", (TUNING*)CigarBox4Tunings, 1}};

const TUNING ForkTunings[] = {ForkCommon, ForkSarti, ForkMid19Century, Fork18Century, ForkVerdi};
const TUNING OtherTunings[] = {ScientificPitch};
const VARIATION MiscellaneousVariations[] = {
    {"Forks", (TUNING*)ForkTunings, 5},
    {"Other", (TUNING*)OtherTunings, 1}};

const INSTRUMENT Instruments[] = {
    {"Guitar", (VARIATION*)GuitarVariations, 2},
    {"Bass", (VARIATION*)BassVariations, 2},
    {"Ukulele", (VARIATION*)UkuleleVariations, 1},
    {"Banjo", (VARIATION*)BanjoVariations, 1},
    {"Cigar Box", (VARIATION*)CigarBoxVariations, 2},
    {"Miscellaneous", (VARIATION*)MiscellaneousVariations, 2}};

#define INSTRUMENTS_COUNT 6
