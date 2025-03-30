#include "gameTime.hpp"

#include <cubos/core/ecs/reflection.hpp>
#include <cubos/core/reflection/external/primitives.hpp>

#include <cubos/engine/input/plugin.hpp>
#include <cubos/engine/transform/plugin.hpp>

using namespace cubos::engine;

CUBOS_REFLECT_IMPL(GameTime)
{
    return cubos::core::ecs::TypeBuilder<GameTime>("GameTime")
        .withField("elapsedTime", &GameTime::elapsedTime)
        .build();
}

void gameTimePlugin(cubos::engine::Cubos& cubos) {

    cubos.resource<GameTime>();

    cubos.system("update time").call([](GameTime& gt, const DeltaTime& dt) {

        CUBOS_INFO("Updating elapsed time: {}", gt.elapsedTime );
        gt.elapsedTime += dt.value();

    });

}
