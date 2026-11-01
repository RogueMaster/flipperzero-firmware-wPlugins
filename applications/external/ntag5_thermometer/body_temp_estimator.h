#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Where the sensor patch is worn */
typedef enum {
    BodyTempLocationArm,
    BodyTempLocationChest,
    BodyTempLocationAbdomen,
    BodyTempLocationCustom,

    BodyTempLocationCount,
} BodyTempLocation;

const char* body_temp_location_name(BodyTempLocation location);

/** Estimate the body temperature from the skin temperature measured by the patch
 *
 * Piecewise linear model of the original phone app (offline path, used when its cloud
 * service gives no estimate). Chest has its own curve, all other locations use the arm curve.
 * Outside the modelled range the skin temperature is returned unchanged.
 *
 * @param      skin      skin temperature in degrees Celsius
 * @param      location  where the patch is worn
 * @return     estimated body temperature in degrees Celsius
 */
float body_temp_estimate(float skin, BodyTempLocation location);

#ifdef __cplusplus
}
#endif
