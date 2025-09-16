#include "log/log.h"
#include "hiddev/hidapidev.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring> // for memset

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
    int HidApiDev::hidApiInitialized = 0;

    HidApiDev::HidApiDev(const uint16_t& _vId, const uint16_t _pId, const int& _interfaceNumber, int readTimeoutUs)
        : vId(_vId),pId(_pId),dev(nullptr),timeout(readTimeoutUs), interfaceNumber(_interfaceNumber)
    { 
        if(hidApiInitialized == 0)
        {
            if(hid_init() < 0)
                throw std::runtime_error("Error: HIDAPI initialization failed.");
        }
        ++hidApiInitialized;
    }

    HidApiDev::~HidApiDev()
    {
        Close();

        --hidApiInitialized;
        if(hidApiInitialized <= 0)
        {
            hidApiInitialized = 0;
            hid_exit();
        }
    }

    bool HidApiDev::Open()
    {
        if(dev != nullptr)
            Close();

        auto info = hid_enumerate(vId,pId);

        while(info != nullptr)
        {
            if(info->interface_number == interfaceNumber)
            {
                dev = hid_open_path(info->path);
                if(dev != nullptr)
                {
                    hid_set_nonblocking(dev,0);
                    return true;
                }
            }

            info = info->next;
        }

        return false;
    }

    bool HidApiDev::Close()
    {
        if(dev != nullptr)
            hid_close(dev);
        dev = nullptr;
        return true;
    }

    bool HidApiDev::IsOpen()
    {
        return dev != nullptr;
    }

    int HidApiDev::Read(std::vector<char> & data)
    {
        if(dev == nullptr)
            return 0;

        int readCnt = 0;

        do {
            auto readCntLoc = hid_read_timeout_dummy((unsigned char*)(data.data()+readCnt),data.size()-readCnt,timeout);
            if(readCntLoc < 0)
                return readCntLoc;
            if(readCntLoc == 0)
                return (readCnt==0)?-1:readCnt;
            readCnt += readCntLoc;
        }
        while(readCnt < data.size());

        return readCnt;
    }

    bool HidApiDev::Write(std::vector<char> & data)
    {
        if(dev == nullptr)
            return false;

        auto writeCnt = hid_write(dev,(unsigned char*)(data.data()),data.size());

        return writeCnt == data.size();
    }

    bool HidApiDev::Write(std::vector<unsigned char> & data)
    {
        if(dev == nullptr)
            return false;

        auto writeCnt = hid_write(dev,data.data(),data.size());

        return writeCnt == data.size();
    }

    bool HidApiDev::EnableGyro()
    {                              
        std::vector<unsigned char> cmd = {   0x00
                                    , 0x87, 0x0f, 0x30, 0x18, 0x00, 0x07, 0x07, 0x00, 0x08, 0x07, 0x00, 0x31, 0x02, 0x00, 0x18, 0x00
                                    , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                    , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                    , 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

        return Write(cmd);
    }
}
