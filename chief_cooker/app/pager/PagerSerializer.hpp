#pragma once

#include "ProtocolAndDecoderProvider.hpp"
#include "lib/String.hpp"
#include "lib/file/FileManager.hpp"
#include "lib/file/FlipperFile.hpp"

#include "data/StoredPagerData.hpp"
#include "lib/hardware/subghz/FrequencyManager.hpp"
#include "protocol/PagerProtocol.hpp"
#include "decoder/PagerDecoder.hpp"

#define KEY_PAGER_STATION_NAME "StationName"
#define KEY_PAGER_FREQUENCY    "Frequency"
#define KEY_PAGER_PROTOCOL     "Protocol"
#define KEY_PAGER_DECODER      "Decoder"
#define KEY_PAGER_DATA         "Data"
#define KEY_PAGER_TE           "TE"

#define NAME_MIN_LENGTH 2
#define NAME_MAX_LENGTH 20

class PagerSerializer {
private:
public:
    String* GetFilename(StoredPagerData* pager) {
        return new String("%06X.fff", pager->data);
    }

    void SavePagerData(
        FileManager* fileManager,
        const char* dir,
        const char* stationName,
        StoredPagerData* pager,
        PagerDecoder* decoder,
        PagerProtocol* protocol,
        uint32_t frequency
    ) {
        String* fileName = GetFilename(pager);
        FlipperFile* stationFile = fileManager->OpenWrite(dir, fileName->cstr());

        stationFile->WriteString(KEY_PAGER_STATION_NAME, stationName);
        stationFile->WriteUInt32(KEY_PAGER_FREQUENCY, frequency);
        stationFile->WriteString(KEY_PAGER_PROTOCOL, protocol->GetSystemName());
        stationFile->WriteString(KEY_PAGER_DECODER, decoder->GetShortName());
        stationFile->WriteUInt32(KEY_PAGER_TE, pager->te);
        stationFile->WriteHex(KEY_PAGER_DATA, pager->data);

        delete stationFile;
        delete fileName;
    }

    String* LoadOnlyStationName(FileManager* fileManager, const char* dir, StoredPagerData* pager) {
        String* filename = GetFilename(pager);
        FlipperFile* stationFile = fileManager->OpenRead(dir, filename->cstr());
        delete filename;

        String* stationName = NULL;
        if(stationFile != NULL) {
            stationName = new String();
            stationFile->ReadString(KEY_PAGER_STATION_NAME, stationName);
            delete stationFile;
        }
        return stationName;
    }

    StoredPagerData LoadPagerData(
        FileManager* fileManager,
        String* stationName,
        const char* dir,
        const char* fileName,
        ProtocolAndDecoderProvider* pdProvider
    ) {
        FlipperFile* stationFile = fileManager->OpenRead(dir, fileName);

        uint32_t te = 0;
        uint64_t hex = 0;
        uint32_t frequency = 0;
        String protocolName;
        String decoderName;

        stationFile->ReadString(KEY_PAGER_STATION_NAME, stationName);
        stationFile->ReadUInt32(KEY_PAGER_FREQUENCY, &frequency);
        stationFile->ReadString(KEY_PAGER_PROTOCOL, &protocolName);
        stationFile->ReadString(KEY_PAGER_DECODER, &decoderName);
        stationFile->ReadUInt32(KEY_PAGER_TE, &te);
        stationFile->ReadHex(KEY_PAGER_DATA, &hex);

        delete stationFile;

        StoredPagerData pager;
        pager.data = hex;
        pager.te = te;
        pager.edited = false;
        pager.frequency = FrequencyManager::GetInstance()->GetFrequencyIndex(frequency);
        pager.protocol = pdProvider->GetProtocolByName(protocolName.cstr())->id;
        pager.decoder = pdProvider->GetDecoderByName(decoderName.cstr())->id;
        return pager;
    }
};
