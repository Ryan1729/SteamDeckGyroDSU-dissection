#include "hiddev/hiddevreader.h"
#include "log/log.h"

#include <sstream>

using namespace kmicki::log;

namespace kmicki::hiddev
{
    // Constants
    const int HidDevReader::cInputRecordLen = 8;    // Number of bytes that are read from hiddev file per 1 byte of HID data.
    const int HidDevReader::cByteposInput = 4;      // Position in the raw hiddev record (of INPUT_RECORD_LEN length) where
                                                    // HID data byte is.

    void HandleMissedTicks(std::string name, std::string tickName, bool received, int & ticks, int period, int & nonMissed)
    {
        if(GetLogLevel() < LogLevelDebug)
            return;
        if(!received)
        {
            if(ticks == 1)
                { LogF(LogLevelDebug) << name << ": Start missing " << tickName << "."; }
            ++ticks;
            if(ticks % period == 0)
            {
                { LogF(LogLevelDebug) << name << ": Missed " << period << " " << tickName << " after " << nonMissed << " " << tickName << ". Still being missed."; }
                if(ticks > period)
                    ticks -= period;
            }
        }
        else if(ticks > period)
        {
            { LogF(LogLevelDebug) << name << ": Missed " << ((ticks+1) % period - 1) << " " << tickName << ". Not being missed anymore."; }
            ticks = 0;
            nonMissed = 0;
        }
        else if(ticks > 0)
        {
            { LogF(LogLevelDebug) << name << ": Missed " << ticks << " " << tickName << " after " << nonMissed << " " << tickName << "."; }
            ticks = 0;
            nonMissed = 0;
        }
        else
            ++nonMissed;
    }

    // Definition - HidDevReader

    void HidDevReader::AddOperation(Thread * operation)
    {
        pipeline.emplace_back(operation);
    }

    HidDevReader::HidDevReader(uint16_t const& vId, uint16_t const& pId, int const& interfaceNumber ,int const& _frameLen, int const& scanTimeUs)
    : frameLen(_frameLen), startStopMutex()
    {
        readDataApi = new ReadDataApi(vId, pId, interfaceNumber, _frameLen, scanTimeUs);

        auto* readDataOp = readDataApi;

        ServeFrame* serveFrame = new ServeFrame(readDataOp->Data);

        AddOperation(readDataOp);
        AddOperation(serveFrame);

        serve = serveFrame;
        readData = readDataOp;

        Log("HidDevReader: Pipeline initialized. Waiting for start...",LogLevelDebug);
    }
}
