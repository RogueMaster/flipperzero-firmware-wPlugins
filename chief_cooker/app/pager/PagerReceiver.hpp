#pragma once

#include "ProtocolAndDecoderProvider.hpp"
#include <cstring>
#include <forward_list>

#include "lib/hardware/subghz/FrequencyManager.hpp"
#include "lib/hardware/subghz/data/SubGhzReceivedData.hpp"

#include "app/AppConfig.hpp"

#include "data/ReceivedPagerData.hpp"
#include "data/KnownStationData.hpp"

#include "protocol/PrincetonProtocol.hpp"
#include "protocol/Smc5326Protocol.hpp"

#include "decoder/Td157Decoder.hpp"
#include "decoder/Td165Decoder.hpp"
#include "decoder/Td174Decoder.hpp"
#include "decoder/L8RDecoder.hpp"
#include "decoder/L8SDecoder.hpp"

#undef LOG_TAG
#define LOG_TAG "PGR_RCV"

#define MAX_REPEATS                  99
#define PAGERS_ARRAY_SIZE_MULTIPLIER 8

using namespace std;

class PagerReceiver : public ProtocolAndDecoderProvider {
public:
    static const uint8_t protocolsCount = 2;
    PagerProtocol* protocols[protocolsCount]{
        new PrincetonProtocol(),
        new Smc5326Protocol(),
    };

    static const uint8_t decodersCount = 5;
    PagerDecoder* decoders[decodersCount]{
        new Td157Decoder(),
        new Td165Decoder(),
        new Td174Decoder(),
        new L8RDecoder(),
        new L8SDecoder(),
    };

private:
    AppConfig* config;
    uint16_t nextPagerIndex = 0;
    uint16_t pagersArraySize = PAGERS_ARRAY_SIZE_MULTIPLIER;
    StoredPagerData* pagers = new StoredPagerData[pagersArraySize];
    size_t knownStationsSize = 0;
    KnownStationData* knownStations;
    uint32_t lastFrequency = 0;
    uint8_t lastFrequencyIndex = 0;
    bool knownStationsLoaded = false;
    const char* userCategory;

    void loadKnownStations() {
        AppFileSysytem appFilesystem;
        forward_list<NamedPagerData> stations;
        bool withNames = config->SavedStrategy == SHOW_NAME;

        size_t count = appFilesystem.GetStationsFromDirectory(&stations, this, User, userCategory, withNames);

        knownStations = new KnownStationData[count];
        for(size_t i = 0; i < count; i++) {
            knownStations[i] = buildKnownStationWithName(stations.front());
            stations.pop_front();
        }

        knownStationsSize = count;
        knownStationsLoaded = true;
    }

    void unloadKnownStations() {
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].name != NULL) {
                delete knownStations[i].name;
            }
        }

        delete[] knownStations;

        knownStationsLoaded = false;
        knownStationsSize = 0;
    }

    KnownStationData buildKnownStationWithName(NamedPagerData pager) {
        KnownStationData data = KnownStationData();
        data.frequency = pager.storedData.frequency;
        data.protocol = pager.storedData.protocol;
        data.decoder = pager.storedData.decoder;
        data.station = decoders[pager.storedData.decoder]->GetStation(pager.storedData.data);
        data.name = pager.name;
        return data;
    }

    KnownStationData buildKnownStationWithoutName(StoredPagerData* pager) {
        KnownStationData data = KnownStationData();
        data.frequency = pager->frequency;
        data.protocol = pager->protocol;
        data.decoder = pager->decoder;
        data.station = decoders[pager->decoder]->GetStation(pager->data);
        data.name = NULL;
        return data;
    }

    PagerDecoder* getDecoder(StoredPagerData* pagerData) {
        for(size_t i = 0; i < decodersCount; i++) {
            pagerData->decoder = i;
            if(IsKnown(pagerData)) {
                return decoders[i];
            }
        }

        for(size_t i = 0; i < decodersCount; i++) {
            if(decoders[i]->GetPager(pagerData->data) <= config->MaxPagerForBatchOrDetection) {
                return decoders[i];
            }
        }

        return decoders[0];
    }

    void addPager(StoredPagerData data) {
        if(nextPagerIndex == pagersArraySize) {
            pagersArraySize += PAGERS_ARRAY_SIZE_MULTIPLIER;
            StoredPagerData* newPagers = new StoredPagerData[pagersArraySize];
            for(int i = 0; i < nextPagerIndex; i++) {
                newPagers[i] = pagers[i];
            }
            delete[] pagers;
            pagers = newPagers;
        }
        pagers[nextPagerIndex++] = data;
    }

public:
    PagerReceiver(AppConfig* config) {
        this->config = config;

        for(size_t i = 0; i < protocolsCount; i++) {
            protocols[i]->id = i;
        }

        for(size_t i = 0; i < decodersCount; i++) {
            decoders[i]->id = i;
        }

        SetUserCategory(config->CurrentUserCategory);
    }

    void SetUserCategory(String* category) {
        SetUserCategory(category != NULL ? category->cstr() : NULL);
    }

    const char* GetCurrentUserCategory() {
        return userCategory;
    }

    void SetUserCategory(const char* category) {
        userCategory = category;
    }

    PagerProtocol* GetProtocolByName(const char* systemProtocolName) {
        for(size_t i = 0; i < protocolsCount; i++) {
            if(strcmp(systemProtocolName, protocols[i]->GetSystemName()) == 0) {
                return protocols[i];
            }
        }

        return NULL;
    }

    PagerDecoder* GetDecoderByName(const char* shortName) {
        for(size_t i = 0; i < decodersCount; i++) {
            if(strcmp(shortName, decoders[i]->GetShortName()) == 0) {
                return decoders[i];
            }
        }

        return NULL;
    }

    void ReloadKnownStations() {
        unloadKnownStations();
        loadKnownStations();
    }

    void LoadStationsFromDirectory(
        CategoryType categoryType,
        const char* category,
        function<void(ReceivedPagerData*)> pagerHandler
    ) {
        AppFileSysytem appFilesystem;
        forward_list<NamedPagerData> stations;
        bool withNames = !knownStationsLoaded && config->SavedStrategy == SHOW_NAME;

        int count = appFilesystem.GetStationsFromDirectory(&stations, this, categoryType, category, withNames);

        delete[] pagers;
        pagers = new StoredPagerData[count];

        if(!knownStationsLoaded) {
            knownStations = new KnownStationData[count];
        }

        for(int i = 0; i < count; i++) {
            NamedPagerData pagerData = stations.front();
            pagers[i] = pagerData.storedData;
            if(!knownStationsLoaded) {
                knownStations[i] = buildKnownStationWithName(pagerData);
            }
            stations.pop_front();

            pagerHandler(new ReceivedPagerData(PagerGetter(i), i, true));
        }

        if(!knownStationsLoaded) {
            knownStationsSize = count;
        }

        nextPagerIndex = count;
        pagersArraySize = count;
        knownStationsLoaded = true;
    }

    PagerDataGetter PagerGetter(size_t index) {
        return [this, index]() { return &pagers[index]; };
    }

    String* GetName(StoredPagerData* pager) {
        uint32_t stationId = buildKnownStationWithoutName(pager).toInt();
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].toInt() == stationId) {
                return knownStations[i].name;
            }
        }
        return NULL;
    }

    bool IsKnown(StoredPagerData* pager) {
        uint32_t stationId = buildKnownStationWithoutName(pager).toInt();
        for(size_t i = 0; i < knownStationsSize; i++) {
            if(knownStations[i].toInt() == stationId) {
                return true;
            }
        }
        return false;
    }

    ReceivedPagerData* Receive(SubGhzReceivedData* data) {
        PagerProtocol* protocol = GetProtocolByName(data->GetProtocolName());
        if(protocol == NULL) {
            return NULL;
        }

        int index = -1;
        uint32_t dataHash = data->GetHash();

        for(size_t i = 0; i < nextPagerIndex; i++) {
            if(pagers[i].data == dataHash && pagers[i].protocol == protocol->id) {
                if(pagers[i].repeats < MAX_REPEATS) {
                    pagers[i].repeats++;
                } else {
                    return NULL; // no need to modify element any more
                }
                index = i;
                break;
            }
        }

        bool isNew = index < 0;
        if(isNew) {
            if(data->GetFrequency() != lastFrequency) {
                lastFrequencyIndex = FrequencyManager::GetInstance()->GetFrequencyIndex(data->GetFrequency());
                lastFrequency = data->GetFrequency();
            }

            StoredPagerData storedData = StoredPagerData();
            storedData.data = dataHash;
            storedData.protocol = protocol->id;
            storedData.repeats = 1;
            storedData.te = data->GetTE();
            storedData.frequency = lastFrequencyIndex;
            storedData.decoder = getDecoder(&storedData)->id;
            storedData.edited = false;

            if(config->SavedStrategy == HIDE && IsKnown(&storedData)) {
                return NULL;
            }

            if(config->AutosaveFoundSignals) {
                AppFileSysytem().AutoSave(&storedData, decoders[storedData.decoder], protocol, lastFrequency);
            }

            index = nextPagerIndex;
            addPager(storedData);
        }

        return new ReceivedPagerData(PagerGetter(index), index, isNew);
    }

    ~PagerReceiver() {
        for(PagerProtocol* protocol : protocols) {
            delete protocol;
        }

        for(PagerDecoder* decoder : decoders) {
            delete decoder;
        }

        delete[] pagers;
        unloadKnownStations();
    }
};
