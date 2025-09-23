#include "cemuhook/cemuhookserver.h"
#include "log/log.h"

#include "cemuhook/cemuhookprotocol.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <algorithm>

#include <iostream>
#include <iomanip>
#include <cmath>

using namespace kmicki::cemuhook::protocol;
using namespace kmicki::log;

#define PORT 26760
#define BUFLEN 100
#define SCANTIME 0
#define DECKSLOT 0
#define SENDTIMEOUT_X 3

#define VERSION_TYPE 0x100000
#define INFO_TYPE 0x100001
#define DATA_TYPE 0x100002

#define SD_SCANTIME_US 4000
#define ACC_1G 0x4000
#define GYRO_1DEGPERSEC 16
#define GYRO_DEADZONE 8
#define ACCEL_SMOOTH 0x1FF

namespace kmicki::cemuhook
{
    const char * GetIP(sockaddr_in const& addr, char *buf)
    {
        return inet_ntop(addr.sin_family,&(addr.sin_addr.s_addr),buf,INET6_ADDRSTRLEN);
    }

    uint32_t crc32(const unsigned char *s,size_t n) {
        uint32_t crc=0xFFFFFFFF;
        
        int k;

        while (n--) {
            crc ^= *s++;
            for (k = 0; k < 8; k++)
                crc = crc & 1 ? (crc >> 1) ^ 0xedb88320  : crc >> 1;
        }
        return ~crc;
    }

    Server::Server()
        : stop(false), serverThread(), stopSending(false),
          mainMutex(), stopSendMutex(), socketSendMutex()
    {
        PrepareAnswerConstants();
        Start();
    }
    
    Server::~Server()
    {
        if(serverThread.get() != nullptr)
        {
            {
                std::lock_guard lock(mainMutex);
                stop = true;
            }
            serverThread.get()->join();
        }
        if(socketFd > -1)
            close(socketFd);
    }

    void Server::Start() 
    {
        Log("Server: Initializing.");
        if(serverThread.get() != nullptr)
        {
            {
                std::lock_guard lock(mainMutex);
                stop = true;
            }
            serverThread.get()->join();
            serverThread.reset();
        }

        socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        timeval read_timeout;
        read_timeout.tv_sec = 2;
        read_timeout.tv_usec = 0;
        setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

        if(socketFd == -1)
            throw std::runtime_error("Server: Socket could not be created.");
        
        sockaddr_in sockInServer;

        sockInServer = sockaddr_in();

        sockInServer.sin_family = AF_INET;
        sockInServer.sin_port = htons(PORT);
        sockInServer.sin_addr.s_addr = INADDR_ANY;

        if(bind(socketFd, (sockaddr*)&sockInServer, sizeof(sockInServer)) < 0)
            throw std::runtime_error("Server: Bind failed.");

        char ipStr[INET6_ADDRSTRLEN];
        ipStr[0] = 0;
        { LogF() << "Server: Socket created at IP: " << GetIP(sockInServer,ipStr) << " Port: " << ntohs(sockInServer.sin_port) << "."; }

        stop = false;
        serverThread.reset(new std::thread(&Server::serverTask,this));
        Log("Server: Initialized.",LogLevelDebug);
    }

    void Server::PrepareAnswerConstants()
    {
        Log("Server: Pre-filling messages.",LogLevelTrace);
        Header outHeader;
        outHeader.magic[0] = 'D';
        outHeader.magic[1] = 'S';
        outHeader.magic[2] = 'U';
        outHeader.magic[3] = 'S';
        outHeader.version = 1001;

        versionAnswer.header = outHeader;
        versionAnswer.header.length = sizeof(versionAnswer.version) + 4;
        versionAnswer.version = 1001;

        SharedResponse sresponse;
        sresponse.deviceModel = 2;
        sresponse.connection = 1;
        sresponse.mac1 = 0;
        sresponse.mac2 = 0;
        sresponse.battery = 0;
        sresponse.connected = 0;

        infoDeckAnswer.header = outHeader;
        infoDeckAnswer.header.eventType = INFO_TYPE;
        infoDeckAnswer.header.length = sizeof(infoDeckAnswer.response) + 4;
        infoDeckAnswer.response = sresponse;
        infoDeckAnswer.response.slot = DECKSLOT;

        infoNoneAnswer = infoDeckAnswer;
        infoNoneAnswer.response.deviceModel = 0;
        infoNoneAnswer.response.connection = 0;
        infoNoneAnswer.response.slotState = 0;

        dataAnswer.header = outHeader;
        dataAnswer.header.eventType = DATA_TYPE;
        dataAnswer.header.length = sizeof(dataAnswer) - 16;
        dataAnswer.response = sresponse;
        dataAnswer.response.slotState = 2;
        dataAnswer.response.connected = 1;
        dataAnswer.response.slot = DECKSLOT;
        
        char* dataAnswerPointer = reinterpret_cast<char*>(&dataAnswer.buttons1);
        auto len = sizeof(DataEvent) - sizeof(Header) - sizeof(SharedResponse) - sizeof(MotionData);
        for (int i = 0; i < len; i++)
        {
            // clear most data
            dataAnswerPointer[i] = 0;
        }
    }

    ssize_t SendPacket(int const& socketFd, std::pair<uint16_t , void const*> const& outBuf, sockaddr_in const& sockInClient)
    {
        // ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
        return sendto(socketFd,outBuf.second,outBuf.first,0,reinterpret_cast<sockaddr*>(const_cast<sockaddr_in*>(&sockInClient)), sizeof(sockInClient));
    }

    void Server::serverTask()
    {
        char buf[BUFLEN];
        sockaddr_in sockInClient;
        socklen_t sockInLen = sizeof(sockInClient);

        auto headerSize = (ssize_t)sizeof(Header);

        std::pair<uint16_t , void const*> outBuf;

        std::unique_ptr<std::thread> sendThread;

        char ipStr[INET6_ADDRSTRLEN];
        ipStr[0] = 0;

        Log("Server: Start listening for client.");
        
        std::unique_lock mainLock(mainMutex);
        while(!stop)
        {
            mainLock.unlock();

            auto recvLen = recvfrom(socketFd,buf,BUFLEN,0,(sockaddr*) &sockInClient, &sockInLen);
            if(recvLen >= headerSize)
            {                
                Header & header = *reinterpret_cast<Header*>(buf);

                std::ostringstream addressTextStream;
                addressTextStream << "IP: " << GetIP(sockInClient,ipStr) << " Port: " << ntohs(sockInClient.sin_port);
                auto addressText = addressTextStream.str();

                switch(header.eventType)
                {
                    case VERSION_TYPE:
                        { LogF(LogLevelTrace) << "Server: A client asked for version. " << addressText << "."; }
                        outBuf = PrepareVersionAnswer(header.id);
                        {
                            std::lock_guard lock(socketSendMutex);
                            SendPacket(socketFd,outBuf,sockInClient);
                        }
                        break;
                    case INFO_TYPE:
                        { LogF(LogLevelTrace) << "Server: A client asked for controller info. " << addressText << "."; }
                        {
                            InfoRequest & req = *reinterpret_cast<InfoRequest*>(buf+headerSize);
                            for (int i = 0; i < req.portCnt; i++)
                            {
                                outBuf = PrepareInfoAnswer(header.id, req.slots[i]);
                                {
                                    std::lock_guard lock(socketSendMutex);
                                    SendPacket(socketFd,outBuf,sockInClient);
                                }
                            }
                        }
                        break;
                    case DATA_TYPE:
                        { LogF(LogLevelTrace) << "Server: A client sent a DATA_TYPE request. " << addressText << "."; }
                        {                           
                            std::shared_lock sharedLock(clientsMutex);
                            auto client = std::find(clients.begin(),clients.end(),sockInClient);
                            if(client == clients.end())
                            {
                                { LogF(LogLevelTrace) << "Server: Request for data from new client. " << addressText << "."; }
                                sharedLock.unlock();
                                {
                                    std::lock_guard lock(clientsMutex);
                                    auto& newClient = clients.emplace_back();
                                    newClient.address = sockInClient;
                                    newClient.id = header.id;
                                }
                                { LogF() << "Server: New client subscribed. " << addressText << "."; }

                                if(sendThread.get() == nullptr)
                                {
                                    stopSending = false;
                                    sendThread.reset(new std::thread(&Server::sendTask,this));
                                }
                            }
                        }
                        break;
                }
            }
            mainLock.lock();
        }

        if(sendThread.get() != nullptr)
        {
            Log("Server: Stopping send thread...",LogLevelDebug);
            {
                std::lock_guard lock(stopSendMutex);
                stopSending = true;
            }
            sendThread.get()->join();
        }
        Log("Server: Stopped.");
    }

    void Server::sendTask()
    {
        static const uint32_t cTimeoutIncreasePeriod = 500;

        Log("Server: Initiating frame grab start.",LogLevelDebug);

        std::pair<uint16_t , void const*> outBuf;
        uint32_t packet = 0;

        Log("Server: Start sending controller data.",LogLevelDebug);

        std::unique_lock mainLock(stopSendMutex);

        uint32_t frameIncrement = 0;

        while(!stopSending)
        {
            mainLock.unlock();
            
            static const uint16_t len = sizeof(dataAnswer);
            dataAnswer.header.id = 0;
            dataAnswer.packetNumber = ++packet;
            
            frameIncrement += 400;
            
            // Set the MotionData to dummy values {
            dataAnswer.motion.timestampL = frameIncrement;

            float t = static_cast<float>(dataAnswer.motion.timestampL) / 1'000'000.0f;

            static const float scale = 45.0f;

            dataAnswer.motion.accX = std::sin(t) * scale;
            dataAnswer.motion.accY = std::cos(t) * scale;
            dataAnswer.motion.accZ = std::sin(t + 0.5) * scale;

            static const float g = 9.81f;

            dataAnswer.motion.pitch = g * std::sin(t);
            dataAnswer.motion.yaw = g * std::cos(t);
            dataAnswer.motion.roll = 0.0f;
            // }            
            
            outBuf = std::pair<uint16_t , void const*>(len, reinterpret_cast<void *>(&dataAnswer));
            
            {
                std::shared_lock lock(clientsMutex);
                for(auto const& client : clients)
                {
                    dataAnswer.header.id = client.id;
                    CalcCrcDataAnswer();
                    {
                        std::lock_guard lock2(socketSendMutex);
                        SendPacket(socketFd,outBuf,client.address);
                    }
                }
            }

            mainLock.lock();
        }

        Log("Server: Stop sending controller data.",LogLevelDebug);
    }


    std::pair<uint16_t , void const*> Server::PrepareVersionAnswer(uint32_t const& id)
    {
        static const uint16_t len = sizeof(versionAnswer);

        versionAnswer.header.id = id;
        versionAnswer.header.crc32 = 0;
        versionAnswer.header.crc32 = crc32(reinterpret_cast<unsigned char *>(&versionAnswer),len);
            return std::pair<uint16_t , void const*>(len, reinterpret_cast<void *>(&versionAnswer));
    }

    std::pair<uint16_t , void const*> Server::PrepareInfoAnswer(uint32_t const& id, uint8_t const& slot)
    {
        static const uint16_t len = sizeof(infoNoneAnswer);

        if(slot != DECKSLOT)
        {
            infoNoneAnswer.header.id = id;
            infoNoneAnswer.response.slot = slot;
            infoNoneAnswer.header.crc32 = 0;
            infoNoneAnswer.header.crc32 = crc32(reinterpret_cast<unsigned char *>(&infoNoneAnswer),len);
            { LogF() << "CRC " << infoNoneAnswer.header.crc32 << "."; }
            return std::pair<uint16_t , void const*>(len, reinterpret_cast<void *>(&infoNoneAnswer));
        }
        
        infoDeckAnswer.header.id = id;
        infoDeckAnswer.response.slot = slot;
        infoDeckAnswer.response.slotState = 2; // The controller is connected
        infoDeckAnswer.header.crc32 = 0;
        infoDeckAnswer.header.crc32 = crc32(reinterpret_cast<unsigned char *>(&infoDeckAnswer),len);
        { LogF() << "CRC " << infoDeckAnswer.header.crc32 << "."; }
        return std::pair<uint16_t , void const*>(len, reinterpret_cast<void *>(&infoDeckAnswer));
    }

    void Server::CalcCrcDataAnswer()
    {
        static const uint16_t len = sizeof(dataAnswer);

        dataAnswer.header.crc32 = 0;
        dataAnswer.header.crc32 = crc32(reinterpret_cast<unsigned char *>(&dataAnswer),len);
    }

    bool Server::Client::operator==(sockaddr_in const& other)
    {
        return address.sin_addr.s_addr == other.sin_addr.s_addr
        && address.sin_port == other.sin_port;
    }

    bool Server::Client::operator!=(sockaddr_in const& other)
    {
        return !(*this == other);
    }
}
