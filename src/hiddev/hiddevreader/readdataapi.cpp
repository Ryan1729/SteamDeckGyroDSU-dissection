#include "hiddev/hiddevreader.h"
#include "hiddev/hidapidev.h"
#include "log/log.h"
#include <hidapi/hidapi.h>

using namespace kmicki::log;

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
                // Simulate blocking for the requested timeout
                if (timeout > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                }
                
                // data ptr
                unsigned char* d = (unsigned char*)(readData.data()+readCnt);
                size_t length = readData.size()-readCnt;
                
                static unsigned char counter = 0;
                for (size_t i = 0; i < length; ++i) {
                    d[i] = static_cast<unsigned char>(counter + i);
                }
                counter++;
                
                readCnt += static_cast<int>(length);
            }
            while(readCnt < readData.size());

            Data.SendData();
        }
        
        Log("HidDevReader::ReadDataApi: Stopped.",LogLevelDebug);
    }
}
