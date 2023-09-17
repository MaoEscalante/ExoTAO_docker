#pragma once

#include <string.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <stdio.h>
#include <unistd.h>

#include "candefines.h"
#include <unordered_map>
#include <mutex>
#include <queue>

using namespace std;

int printFrame(can_frame_s frame);

class CanNetwork
{
private:
    char _iface[10];
    int _s;
    struct sockaddr_can _addr;
    struct ifreq _ifr;
    std::unordered_map<int, can_frame_s> _dic;

    mutex _mtx_dic;
    mutex _mtx_write;
    mutex _mtx_write_queue;

public:
    std::queue<can_frame_s> _queued_frames;

    CanNetwork(char *iface);
    CanNetwork();

    int connect();
    int sendFrameAsync(can_frame_s frame);
    int sendFrame(can_frame_s frame, bool async = false);
    int disconnect();
    int recFrame(can_frame_s &frame);

    int writeAsync()
    {   
        can_frame_s frame;
        while (!_queued_frames.empty())
        {
            {
                std::unique_lock<std::mutex> lock(_mtx_write_queue);
                frame = _queued_frames.front();
                _queued_frames.pop();
            }
            // while (sendFrame(frame,false));
            if(sendFrame(frame,false) == 0){
                
            };
        }
        return 0;
    }

    void getDicKey(int key, can_frame_s &frame)
    {
        std::unique_lock<std::mutex> lock(_mtx_dic);
        frame = _dic[key];
    }

    void fillDic()
    {
        can_frame_s frame;
        int nbytes;
        {
            std::unique_lock<std::mutex> lock(_mtx_dic);

            while ((nbytes = read(_s, &frame, sizeof(can_frame_s))) > 0)
            {
                _dic[frame.can_id] = frame;
            }
        }
    }
    void setIface(char *iface);

    ~CanNetwork();
};
