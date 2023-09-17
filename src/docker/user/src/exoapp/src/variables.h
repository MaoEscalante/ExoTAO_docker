#pragma once

#include <cannetwork.hpp>
#include <eposnetwork.hpp>
#include <eposnode.hpp>

extern CanNetwork can;

namespace EXO
{
    EposNetwork net(can);
    
    namespace R
    {
        namespace Knee
        {
            EposNode Motor(4, can);
            EposNode Encoder(3, can);
        };
    };

    namespace L
    {

    };
}

