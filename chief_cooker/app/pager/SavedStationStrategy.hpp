#pragma once

enum SavedStationStrategy {
    IGNORE, // don't check if station is saved, show as unknown
    SHOW_NAME, // show station name instead of hex and station number
    HIDE, // hide all station signals from the search

    SavedStationStrategyValuesCount,
};
