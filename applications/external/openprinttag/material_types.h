#pragma once

// Material class enum from OpenPrintTag specification
// https://github.com/prusa3d/OpenPrintTag/blob/main/data/material_class_enum.yaml

typedef struct {
    uint32_t key;
    const char* abbreviation;
    const char* name;
} MaterialClass;

static const MaterialClass material_classes[] = {
    {0, "FFF", "Filament"},
    {1, "SLA", "Resin"},
};

#define MATERIAL_CLASSES_COUNT (sizeof(material_classes) / sizeof(MaterialClass))

// Get material class abbreviation from enum value
static inline const char* material_class_get_abbr(uint32_t key) {
    for(size_t i = 0; i < MATERIAL_CLASSES_COUNT; i++) {
        if(material_classes[i].key == key) {
            return material_classes[i].abbreviation;
        }
    }
    return "Unknown";
}

// Get material class full name from enum value
static inline const char* material_class_get_name(uint32_t key) {
    for(size_t i = 0; i < MATERIAL_CLASSES_COUNT; i++) {
        if(material_classes[i].key == key) {
            return material_classes[i].name;
        }
    }
    return "Unknown Class";
}

// Material type enum from OpenPrintTag specification
// https://github.com/prusa3d/OpenPrintTag/blob/main/data/material_type_enum.yaml

typedef struct {
    uint32_t key;
    const char* abbreviation;
    const char* name;
} MaterialType;

static const MaterialType material_types[] = {
    {0, "PLA", "Polylactic Acid"},
    {1, "PETG", "Polyethylene Terephthalate Glycol"},
    {2, "TPU", "Thermoplastic Polyurethane"},
    {3, "ABS", "Acrylonitrile Butadiene Styrene"},
    {4, "ASA", "Acrylonitrile Styrene Acrylate"},
    {5, "PC", "Polycarbonate"},
    {6, "PCTG", "Polycyclohexylenedimethylene Terephthalate Glycol"},
    {7, "PP", "Polypropylene"},
    {8, "PA6", "Polyamide 6"},
    {9, "PA11", "Polyamide 11"},
    {10, "PA12", "Polyamide 12"},
    {11, "PA66", "Polyamide 66"},
    {12, "CPE", "Copolyester"},
    {13, "TPE", "Thermoplastic Elastomer"},
    {14, "HIPS", "High Impact Polystyrene"},
    {15, "PHA", "Polyhydroxyalkanoate"},
    {16, "PET", "Polyethylene Terephthalate"},
    {17, "PEI", "Polyetherimide"},
    {18, "PBT", "Polybutylene Terephthalate"},
    {19, "PVB", "Polyvinyl Butyral"},
    {20, "PVA", "Polyvinyl Alcohol"},
    {21, "PEKK", "Polyetherketoneketone"},
    {22, "PEEK", "Polyether Ether Ketone"},
    {23, "BVOH", "Butenediol Vinyl Alcohol Copolymer"},
    {24, "TPC", "Thermoplastic Copolyester"},
    {25, "PPS", "Polyphenylene Sulfide"},
    {26, "PPSU", "Polyphenylsulfone"},
    {27, "PVC", "Polyvinyl Chloride"},
    {28, "PEBA", "Polyether Block Amide"},
    {29, "PVDF", "Polyvinylidene Fluoride"},
    {30, "PPA", "Polyphthalamide"},
    {31, "PCL", "Polycaprolactone"},
    {32, "PES", "Polyethersulfone"},
    {33, "PMMA", "Polymethyl Methacrylate"},
    {34, "POM", "Polyoxymethylene"},
    {35, "PPE", "Polyphenylene Ether"},
    {36, "PS", "Polystyrene"},
    {37, "PSU", "Polysulfone"},
    {38, "TPI", "Thermoplastic Polyimide"},
};

#define MATERIAL_TYPES_COUNT (sizeof(material_types) / sizeof(MaterialType))

// Get material type abbreviation from enum value
static inline const char* material_type_get_abbr(uint32_t key) {
    for(size_t i = 0; i < MATERIAL_TYPES_COUNT; i++) {
        if(material_types[i].key == key) {
            return material_types[i].abbreviation;
        }
    }
    return "Unknown";
}

// Get material type full name from enum value
static inline const char* material_type_get_name(uint32_t key) {
    for(size_t i = 0; i < MATERIAL_TYPES_COUNT; i++) {
        if(material_types[i].key == key) {
            return material_types[i].name;
        }
    }
    return "Unknown Material";
}
