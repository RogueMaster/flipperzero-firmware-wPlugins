/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef UNITEMP_SCD4x
#define UNITEMP_SCD4x

#include "../unitemp.h"
#include "../sensors.h"

bool unitemp_SCD4x_alloc(Sensor* sensor, char* args);
bool unitemp_SCD4x_free(Sensor* sensor);
bool unitemp_SCD4x_init(Sensor* sensor);
bool unitemp_SCD4x_deinit(Sensor* sensor);
SensorStatus unitemp_SCD4x_update(Sensor* sensor);

extern const SensorModel SCD4x;

#endif
