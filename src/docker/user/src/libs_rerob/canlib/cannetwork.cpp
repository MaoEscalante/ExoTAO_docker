#include "cannetwork.hpp"

#include <fcntl.h>

CanNetwork::CanNetwork(char *iface)
{
    setIface(iface);
}

CanNetwork::~CanNetwork()
{
    printf("CanNetwork::~CanNetwork()\n");
}

CanNetwork::CanNetwork()
{
}

void CanNetwork::setIface(char *iface)
{
    strcpy(_iface, iface);
}

int CanNetwork::connect()
{
    if ((_s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("Socket");
        return 1;
    }

    // --------------------------------
    int sndbuf = 1000;
    if (setsockopt(_s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0)
        perror("setsockopt");
    // --------------------------------

    strcpy(_ifr.ifr_name, _iface);
    ioctl(_s, SIOCGIFINDEX, &_ifr);

    memset(&_addr, 0, sizeof(_addr));
    _addr.can_family = AF_CAN;
    _addr.can_ifindex = _ifr.ifr_ifindex;

    if (bind(_s, (struct sockaddr *)&_addr, sizeof(_addr)) < 0)
    {
        perror("Bind");
        return 1;
    }

    fcntl(_s, F_SETFL, O_NONBLOCK);

    return 0;
}

int CanNetwork::disconnect()
{
    if (close(_s) < 0)
    {
        perror("Close");
        return 1;
    }

    return 0;
}

int CanNetwork::sendFrameAsync(can_frame_s frame)
{
    {
        std::unique_lock<std::mutex> lock(_mtx_write_queue);
        _queued_frames.push(frame);
    }

    return 0;
}

int CanNetwork::sendFrame(can_frame_s frame, bool async)
{
    if (async)
    {
        return sendFrameAsync(frame);
    }

    {
        std::unique_lock<std::mutex> lock(_mtx_write);

        if (write(_s, &frame, sizeof(can_frame_s)) != sizeof(can_frame_s))
        {
            perror("[Write]");
            return 1;
        }
    }

    can_tic_toc;
    return 0;
}

int CanNetwork::recFrame(can_frame_s &frame)
{
    int nbytes = read(_s, &frame, sizeof(can_frame_s));

    // if (nbytes > 0) printFrame(frame);

    return nbytes;
}

int printFrame(can_frame_s frame)
{
    printf("Frame:\n"
           "  ID: 0x%x\n"
           "  SZ: %d\n"
           "  Payload: \n    ",
           frame.can_id, frame.can_dlc);

    for (int i = 0; i < frame.can_dlc; i++)
        printf("%02X ", frame.data[i]);
    printf("\n");
    return 0;
}