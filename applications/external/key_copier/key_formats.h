#ifndef KEY_FORMATS_H
#define KEY_FORMATS_H

#define FORMAT_NUM 2

typedef struct {
    char* manufacturer;
    char* format_name;
    char* format_link;

    double first_pin_inch;
    double last_pin_inch;
    double pin_increment_inch;
    int pin_num;
    double pin_width_inch;
    double drill_angle;
    double elbow_inch;

    double uncut_depth_inch;
    double deepest_depth_inch;
    double depth_step_inch;
    int min_depth_ind;
    int max_depth_ind;

    int macs;
    int clearance;
} KeyFormat;

extern const KeyFormat all_formats[FORMAT_NUM];

#endif // KEY_FORMATS_H
