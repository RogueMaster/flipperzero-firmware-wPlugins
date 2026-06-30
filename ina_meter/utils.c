/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

FuriString* format_resistance_value(double value) {
    FuriString* text = furi_string_alloc();
    if(value < (double)1E-6) { // < 1uOhm
        furi_string_printf(text, "%.0fu", (double)1E6 * value);
    } else if(value < (double)10E-3) { // < 10mOhm
        furi_string_printf(text, "%.2fm", (double)1E3 * value);
    } else if(value < (double)100E-3) { // < 100mOhm
        furi_string_printf(text, "%.1fm", (double)1E3 * value);
    } else if(value < 1) { // < 1Ohm
        furi_string_printf(text, "%.0fm", (double)1E3 * value);
    } else if(value < 10) { // < 10Ohm
        furi_string_printf(text, "%.2f", value);
    } else { // >= 10Ohm
        furi_string_printf(text, "%.1f", value);
    }
    return text;
}
