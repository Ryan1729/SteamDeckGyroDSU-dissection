#include "sdgyrodsu/cemuhookadapter.h"
#include "sdgyrodsu/sdhidframe.h"
#include "log/log.h"

#include <iostream>
#include <iomanip>
#include <cmath>

using namespace kmicki::cemuhook::protocol;
using namespace kmicki::log;

#define SD_SCANTIME_US 4000
#define ACC_1G 0x4000
#define GYRO_1DEGPERSEC 16
#define GYRO_DEADZONE 8
#define ACCEL_SMOOTH 0x1FF

namespace kmicki::sdgyrodsu
{
    MotionData & SetTimestamp(MotionData &data, uint64_t const& timestamp)
    {
        data.timestampL = (uint32_t)(timestamp & 0xFFFFFFFF);
        data.timestampH = (uint32_t)(timestamp >> 32);

        return data;
    }

    uint64_t ToTimestamp(uint32_t const& increment)
    {
        return (uint64_t)increment*SD_SCANTIME_US;
    }

    MotionData &  SetTimestamp(MotionData &data, uint32_t const& increment)
    {
        SetTimestamp(data,ToTimestamp(increment));

        return data;
    }

    const int cFrameLen = 64;       // Steam Deck Controls' custom HID report length in bytes
    const int cScanTimeUs = 4000;   // Steam Deck Controls' period between received report data in microseconds
    const uint16_t cVID = 0x28de;   // Steam Deck Controls' USB Vendor-ID
    const uint16_t cPID = 0x1205;   // Steam Deck Controls' USB Product-ID
    const int cInterfaceNumber = 2; // Steam Deck Controls' USB Interface Number

    CemuhookAdapter::CemuhookAdapter()
    : lastInc(0),
      lastAccelRtL(0.0),lastAccelFtB(0.0),lastAccelTtB(0.0),
      isPersistent(true), toReplicate(0), reader(new kmicki::hiddev::HidDevReader(cVID,cPID,cInterfaceNumber,cFrameLen,cScanTimeUs))
    {
        Log("CemuhookAdapter: Initialized. Waiting for start of frame grab.",LogLevelDebug);
    }

    void CemuhookAdapter::StartFrameGrab()
    {
        lastInc = 0;
        ignoreFirst = true;
        Log("CemuhookAdapter: Starting frame grab.",LogLevelDebug);

        std::lock_guard startLock(reader->startStopMutex); // prevent starting and stopping at the same time

        Log("HidDevReader: Attempting to start the pipeline...",LogLevelDebug);

        for (auto& thread : reader->pipeline)
            thread->Start();

        Log("HidDevReader: Started the pipeline.");

        frameServe = &reader->serve->GetServe();
    }

    int const& CemuhookAdapter::SetMotionDataNewFrame(MotionData &motion)
    {
        static const int64_t cMaxDiffReplicate = 100;
        static const int cMaxRepeatedLoop = 1000;

        auto const& dataFrame = frameServe->GetPointer();

        if(ignoreFirst)
        {
            auto lock = frameServe->GetConsumeLock();
            ignoreFirst = false;
        }

        int repeatedLoop = cMaxRepeatedLoop;

        while(true)
        {
            if(toReplicate == 0)
            {
                //Log("DEBUG: TRY GET CONSUME LOCK.");
                auto lock = frameServe->GetConsumeLock();
                //Log("CONSUME LOCK ACQUIRED.");
                auto const& frame = GetSdFrame(*dataFrame);

                if( frame.AccelAxisFrontToBack == 0 && frame.AccelAxisRightToLeft == 0
                    &&  frame.AccelAxisTopToBottom == 0 && frame.GyroAxisFrontToBack == 0
                    &&  frame.GyroAxisRightToLeft == 0 && frame.GyroAxisTopToBottom == 0)
                {
                    NoGyro.SendSignal();
                }

                int64_t diff = (int64_t)frame.Increment - (int64_t)lastInc;

                if(lastInc != 0 && diff < 1 && diff > -100)
                {
                    if(repeatedLoop == cMaxRepeatedLoop)
                    {
                        Log("CemuhookAdapter: Frame was repeated. Ignoring...",LogLevelDebug);
                        { LogF(LogLevelTrace) << std::setw(8) << std::setfill('0') << std::setbase(16)
                                        << "Current increment: 0x" << frame.Increment << ". Last: 0x" << lastInc << "."; }
                    }
                    if(repeatedLoop <= 0)
                    {
                        Log("CemuhookAdapter: Frame is repeated continously...");
                        return toReplicate;
                    }
                    --repeatedLoop;
                }
                else
                {
                    if(lastInc != 0 && diff > 1 && diff <= cMaxDiffReplicate) {
                        toReplicate = diff-1;
                    }

                    // Set the MotionData to dummy values {
                    SetTimestamp(motion, frame.Increment);

                    float t = static_cast<float>(motion.timestampL) / 1'000'000.0f;

                    static const float scale = 45.0f;

                    motion.accX = std::sin(t) * scale;
                    motion.accY = std::cos(t) * scale;
                    motion.accZ = std::sin(t + 0.5) * scale;

                    static const float g = 9.81f;

                    motion.pitch = g * std::sin(t);
                    motion.yaw = g * std::cos(t);
                    motion.roll = 0.0f;
                    // }

                    if(toReplicate > 0)
                    {
                        lastTimestamp = ToTimestamp(lastInc+1);
                        SetTimestamp(motion,lastTimestamp);
                        if(!isPersistent)
                            data = motion;
                    }

                    lastInc = frame.Increment;

                    return toReplicate;
                }
            }
            else
            {
                // Replicated frame
                --toReplicate;
                lastTimestamp += SD_SCANTIME_US;
                if(!isPersistent)
                {
                    motion = SetTimestamp(data,lastTimestamp);
                }
                else
                    SetTimestamp(motion,lastTimestamp);

                return toReplicate;
            }
        }
    }

    void CemuhookAdapter::StopFrameGrab()
    {
        Log("CemuhookAdapter: Stopping frame grab.",LogLevelDebug);

        reader->serve->StopServe(*frameServe);
        frameServe = nullptr;

        std::lock_guard startLock(reader->startStopMutex); // prevent starting and stopping at the same time

        Log("HidDevReader: Attempting to stop the pipeline...",LogLevelDebug);

        for (auto thread = reader->pipeline.rbegin(); thread != reader->pipeline.rend(); ++thread)
            (*thread)->TryStopThenKill(std::chrono::seconds(10));

        Log("HidDevReader: Stopped the pipeline.");
    }

    bool CemuhookAdapter::IsControllerConnected()
    {
        return true;
    }
}
