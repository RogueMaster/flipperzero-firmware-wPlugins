#pragma once

#include <notification/notification.h>
#include "settings.h"

void stratahero_code_glyph_entry_success_notification(NotificationApp* notification, const StrataHeroSettings* settings);
void stratahero_code_glyph_entry_failure_notification(NotificationApp* notification, const StrataHeroSettings* settings);
void stratahero_code_complete_notification(NotificationApp* notification, const StrataHeroSettings* settings);
void stratahero_stats_hit_notification(NotificationApp* notification, const StrataHeroSettings* settings);
void stratahero_stats_final_hit_notification(NotificationApp* notification, const StrataHeroSettings* settings);
