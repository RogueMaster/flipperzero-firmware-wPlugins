#pragma once

#include <storage/storage.h>
#include <forward_list>

#include "app/pager/PagerSerializer.hpp"
#include "lib/file/FileManager.hpp"
#include "pager/data/NamedPagerData.hpp"

// .fff stands for (f)lipper (f)ile (f)ormat
#define CONFIG_FILE_PATH APP_DATA_PATH("config.fff")

#define STATIONS_PATH          APP_DATA_PATH("stations")
#define STATIONS_PATH_OF(path) STATIONS_PATH "/" path

#define SAVED_STATIONS_PATH     STATIONS_PATH_OF("saved")
#define AUTOSAVED_STATIONS_PATH STATIONS_PATH_OF("autosaved")

#define MAX_FILENAME_LENGTH 16

using namespace std;

enum CategoryType {
    User,
    Autosaved,

    NotSelected,
};

class AppFileSysytem {
private:
    String* getCategoryPath(CategoryType categoryType, const char* category) {
        switch(categoryType) {
        case User:
            if(category != NULL) {
                return new String("%s/%s", SAVED_STATIONS_PATH, category);
            } else {
                return new String(SAVED_STATIONS_PATH);
            }

        case Autosaved:
            return new String("%s/%s", AUTOSAVED_STATIONS_PATH, category);

        default:
        case NotSelected:
            return NULL;
        }
    }

    String* getFilePath(CategoryType categoryType, const char* category, StoredPagerData* pager) {
        String* categoryPath = getCategoryPath(categoryType, category);
        String* pagerFilename = PagerSerializer().GetFilename(pager);
        String* filePath = new String("%s/%s", categoryPath->cstr(), pagerFilename->cstr());
        delete categoryPath;
        delete pagerFilename;
        return filePath;
    }

public:
    int GetCategories(forward_list<char*>* categoryList, CategoryType categoryType) {
        const char* dirPath;
        switch(categoryType) {
        case User:
            dirPath = SAVED_STATIONS_PATH;
            break;

        case Autosaved:
            dirPath = AUTOSAVED_STATIONS_PATH;
            break;

        default:
            return 0;
        }

        FileManager fileManager = FileManager();
        Directory* dir = fileManager.OpenDirectory(dirPath);
        uint16_t categoriesLoaded = 0;

        if(dir != NULL) {
            char fileName[MAX_FILENAME_LENGTH];
            while(dir->GetNextDir(fileName, MAX_FILENAME_LENGTH)) {
                char* category = new char[strlen(fileName)];
                strcpy(category, fileName);
                categoryList->push_front(category);
                categoriesLoaded++;
            }
        }

        delete dir;
        return categoriesLoaded;
    }

    size_t GetStationsFromDirectory(
        forward_list<NamedPagerData>* stationList,
        ProtocolAndDecoderProvider* pdProvider,
        CategoryType categoryType,
        const char* category,
        bool loadNames
    ) {
        FileManager fileManager = FileManager();
        String* stationDirPath = getCategoryPath(categoryType, category);
        Directory* dir = fileManager.OpenDirectory(stationDirPath->cstr());
        PagerSerializer serializer = PagerSerializer();
        size_t stationsLoaded = 0;

        if(dir != NULL) {
            char fileName[MAX_FILENAME_LENGTH];
            while(dir->GetNextFile(fileName, MAX_FILENAME_LENGTH)) {
                String* stationName = new String();
                StoredPagerData pager =
                    serializer.LoadPagerData(&fileManager, stationName, stationDirPath->cstr(), fileName, pdProvider);

                if(!loadNames) {
                    delete stationName;
                    stationName = NULL;
                }

                NamedPagerData returnData = NamedPagerData();
                returnData.storedData = pager;
                returnData.name = stationName;
                stationList->push_front(returnData);
                stationsLoaded++;
            }
        }

        delete dir;
        delete stationDirPath;

        return stationsLoaded;
    }

    String* GetOnlyStationName(CategoryType categoryType, const char* category, StoredPagerData* pager) {
        FileManager fileManager = FileManager();
        String* categoryPath = getCategoryPath(categoryType, category);
        String* name = PagerSerializer().LoadOnlyStationName(&fileManager, categoryPath->cstr(), pager);
        delete categoryPath;
        return name;
    }

    void AutoSave(StoredPagerData* storedData, PagerDecoder* decoder, PagerProtocol* protocol, uint32_t frequency) {
        DateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);
        String* todayDate = new String("%d-%02d-%02d", datetime.year, datetime.month, datetime.day);
        String* todaysDir = getCategoryPath(Autosaved, todayDate->cstr());

        FileManager fileManager = FileManager();
        fileManager.CreateDirIfNotExists(STATIONS_PATH);
        fileManager.CreateDirIfNotExists(AUTOSAVED_STATIONS_PATH);
        fileManager.CreateDirIfNotExists(todaysDir->cstr());

        PagerSerializer().SavePagerData(&fileManager, todaysDir->cstr(), "", storedData, decoder, protocol, frequency);

        delete todaysDir;
        delete todayDate;
    }

    void SaveToUserCategory(
        const char* userCategory,
        const char* stationName,
        StoredPagerData* storedData,
        PagerDecoder* decoder,
        PagerProtocol* protocol,
        uint32_t frequency
    ) {
        String* catDir = getCategoryPath(User, userCategory);

        FileManager fileManager = FileManager();
        fileManager.CreateDirIfNotExists(STATIONS_PATH);
        fileManager.CreateDirIfNotExists(AUTOSAVED_STATIONS_PATH);
        fileManager.CreateDirIfNotExists(catDir->cstr());

        PagerSerializer().SavePagerData(&fileManager, catDir->cstr(), stationName, storedData, decoder, protocol, frequency);

        delete catDir;
    }

    void DeletePager(const char* userCategory, StoredPagerData* storedData) {
        String* filePath = getFilePath(User, userCategory, storedData);
        FileManager().DeleteFile(filePath->cstr());
        delete filePath;
    }

    void DeleteCategory(const char* userCategory) {
        String* catPath = getCategoryPath(User, userCategory);
        FileManager().DeleteFile(catPath->cstr());
        delete catPath;
    }
};
