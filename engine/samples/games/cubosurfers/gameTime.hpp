#pragma once

#include <cubos/engine/prelude.hpp>

struct GameTime
{
    CUBOS_REFLECT;

    float elapsedTime{0.0F};
};

void gameTimePlugin(cubos::engine::Cubos& cubos);
