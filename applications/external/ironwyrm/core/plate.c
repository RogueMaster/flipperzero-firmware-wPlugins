#include "plate.h"

#include "data.h"
#include "random.h"

static int Clamp(int value, int minimum, int maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return value;
}

Plate MakePlate(MaterialId materialId, ModifierId modifierId) {
    const Material* material = &materials[materialId];
    const Modifier* modifier = &modifiers[modifierId];

    Plate plate = {
        .materialIndex = (uint8_t)materialId,
        .modifierIndex = (uint8_t)modifierId,
        .integrity = (uint8_t)Clamp(material->integrity + modifier->integrity, 1, 9),
        .power = (uint8_t)Clamp(material->power + modifier->power, 1, 9),
        .price = (uint8_t)Clamp(material->price + modifier->price, 1, 9),
    };

    return plate;
}

Plate MakeRandomPlate(RandomGenerator* randomGenerator) {
    MaterialId materialId =
        (MaterialId)RandomRange(randomGenerator, 0, MaterialIdCount - 1);
    ModifierId modifierId =
        (ModifierId)RandomRange(randomGenerator, 0, ModifierIdCount - 1);

    return MakePlate(materialId, modifierId);
}
