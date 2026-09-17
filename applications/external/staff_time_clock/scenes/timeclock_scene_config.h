// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

// X-macro list of scenes. Each ADD_SCENE(prefix, name, id) expands into an
// enum entry and the three handler-table slots, keeping them all in sync.
ADD_SCENE(timeclock, pin_unlock, PinUnlock)
ADD_SCENE(timeclock, date_warning, DateWarning)
ADD_SCENE(timeclock, onboarding, Onboarding)
ADD_SCENE(timeclock, menu, Menu)
ADD_SCENE(timeclock, work, Work)
ADD_SCENE(timeclock, scan, Scan)
ADD_SCENE(timeclock, name_input, NameInput)
ADD_SCENE(timeclock, badge_list, BadgeList)
ADD_SCENE(timeclock, badge_detail, BadgeDetail)
ADD_SCENE(timeclock, confirm_delete, ConfirmDelete)
ADD_SCENE(timeclock, overview, Overview)
ADD_SCENE(timeclock, history_menu, HistoryMenu)
ADD_SCENE(timeclock, history, History)
ADD_SCENE(timeclock, today, Today)
ADD_SCENE(timeclock, week, Week)
ADD_SCENE(timeclock, month, Month)
ADD_SCENE(timeclock, export, Export)
ADD_SCENE(timeclock, restore, Restore)
ADD_SCENE(timeclock, settings, Settings)
ADD_SCENE(timeclock, about, About)
ADD_SCENE(timeclock, pin_set, PinSet)
