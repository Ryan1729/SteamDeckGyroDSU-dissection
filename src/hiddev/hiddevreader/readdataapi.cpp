#include "hiddev/hiddevreader.h"
#include "hiddev/hidapidev.h"
#include "log/log.h"
#include <hidapi/hidapi.h>

using namespace kmicki::log;

int hid_read_timeout_dummy(unsigned char* data, size_t length, int milliseconds)
{
    // Simulate blocking for the requested timeout
    if (milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    // Fill the buffer with predictable dummy data
    static unsigned char counter = 0;
    for (size_t i = 0; i < length; ++i) {
        data[i] = static_cast<unsigned char>(counter + i);
    }
    counter++;

    // Return "success" with full length read
    return static_cast<int>(length);
}

namespace kmicki::hiddev
{
    static const int cApiScanTimeToTimeout = 2;

    // Definition - ReadDataApi
    HidDevReader::ReadDataApi::ReadDataApi(uint16_t const& _vId, uint16_t const& _pId, const int& _interfaceNumber, int const& _frameLen, int const& _scanTimeUs)
    : vId(_vId), pId(_pId), ReadData(_frameLen), timeout(cApiScanTimeToTimeout*_scanTimeUs/1000),interfaceNumber(_interfaceNumber),noGyro(nullptr)
    { }

    void HidDevReader::ReadDataApi::SetNoGyro(SignalOut &_noGyro)
    {
        noGyro = &_noGyro;
    }
 
    void HidDevReader::ReadDataApi::Execute()
    {
        auto const& data = Data.GetPointerToFill();

        Log("HidDevReader::ReadDataApi: Started.",LogLevelDebug);

        while(ShouldContinue())
        {
            if(!ShouldContinue())
                break;

            if(noGyro && noGyro->TrySignal())
            {
                continue;
            }

            int readCnt = 0;

            std::vector<char> & readData = *data;

            do {
                auto readCntLoc = hid_read_timeout_dummy((unsigned char*)(readData.data()+readCnt),readData.size()-readCnt,timeout);
                if(readCntLoc < 0)
                    break;
                if(readCntLoc == 0)
                    break;
                readCnt += readCntLoc;
            }
            while(readCnt < readData.size());

            Data.SendData();
        }
        
        Log("HidDevReader::ReadDataApi: Stopped.",LogLevelDebug);
    }
}
