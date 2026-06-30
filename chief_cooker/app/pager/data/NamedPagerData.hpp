#pragma once

#include "StoredPagerData.hpp"
#include "lib/String.hpp"

struct NamedPagerData {
    StoredPagerData storedData;
    String* name;
};
