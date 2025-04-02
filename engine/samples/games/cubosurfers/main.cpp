#include <cubos/engine/assets/plugin.hpp>
#include <cubos/engine/collisions/colliding_with.hpp>
#include <cubos/engine/defaults/plugin.hpp>
#include <cubos/engine/input/plugin.hpp>
#include <cubos/engine/render/lights/environment.hpp>
#include <cubos/engine/render/voxels/palette.hpp>
#include <cubos/engine/scene/plugin.hpp>
#include <cubos/engine/settings/plugin.hpp>
#include <cubos/engine/tools/plugin.hpp>
#include <cubos/engine/utils/free_camera/plugin.hpp>
#include <cubos/engine/voxels/plugin.hpp>

#include "obstacle.hpp"
#include "armour.hpp"
#include "player.hpp"
#include "spawner.hpp"
#include "gameTime.hpp"
#include "jetpack.hpp"


using namespace cubos::engine;

static const Asset<Scene> SceneAsset = AnyAsset("/assets/scenes/main.cubos");
static const Asset<VoxelPalette> PaletteAsset = AnyAsset("/assets/main.pal");
static const Asset<InputBindings> InputBindingsAsset = AnyAsset("/assets/input.bind");

int main(int argc, char** argv)
{
    Cubos cubos{argc, argv};

    cubos.plugin(defaultsPlugin);
    cubos.plugin(freeCameraPlugin);
    cubos.plugin(toolsPlugin);
    cubos.plugin(spawnerPlugin);
    cubos.plugin(obstaclePlugin);
    cubos.plugin(armourPlugin);
    cubos.plugin(jetpackPlugin);
    cubos.plugin(playerPlugin);
    cubos.plugin(gameTimePlugin);

    cubos.startupSystem("configure settings").before(settingsTag).call([](Settings& settings) {
        settings.setString("assets.app.osPath", APP_ASSETS_PATH);
        settings.setString("assets.builtin.osPath", BUILTIN_ASSETS_PATH);
    });

    cubos.startupSystem("set the palette, environment, input bindings and spawn the scene")
        .tagged(assetsTag)
        .call([](Commands commands, const Assets& assets, RenderPalette& palette, Input& input,
                 RenderEnvironment& environment) {
            palette.asset = PaletteAsset;
            environment.ambient = {0.1F, 0.1F, 0.1F};
            environment.skyGradient[0] = {0.2F, 0.4F, 0.8F};
            environment.skyGradient[1] = {0.6F, 0.6F, 0.8F};
            input.bind(*assets.read(InputBindingsAsset));
            commands.spawn(assets.read(SceneAsset)->blueprint);
        });

    cubos.system("restart the game on input")
        .call([](Commands cmds, const Assets& assets, const Input& input, Query<Entity> all, GameTime& gt) {
            if (input.justPressed("restart"))
            {
                for (auto [ent] : all)
                {
                    cmds.destroy(ent);
                }

                cmds.spawn(assets.read(SceneAsset)->blueprint);
                gt.elapsedTime = 0.0F;
            }
        });

    cubos.system("Restart on player collision")
        .call([](Commands cmds, const Assets& assets, Query<Player&, const CollidingWith&, Obstacle&> collisions, Query<Entity> all, GameTime& gt) {
            for (auto [player, collidingWith, obstacle] : collisions)
            {
                CUBOS_INFO("Player collided with an obstacle. Armored: {}", player.armored);

                if (player.armored) {
                    obstacle.velocity.z = -10000; // xDDDDDD
                    player.armored = false;
                    break;
                }

                for (auto [ent] : all)
                {
                    cmds.destroy(ent);
                }

                cmds.spawn(assets.read(SceneAsset)->blueprint);
                gt.elapsedTime = 0.0F;
                break;
            }
        });

    cubos.system("Check for armor pickup")
        .call([](Query<Player&, const CollidingWith&, const Armour&> collisions) {

            for (auto [player, collidingWith, armour] : collisions)
            {
                CUBOS_INFO("Armored picked up");
                player.armored = true;
                CUBOS_INFO("Armored = {}", player.armored);
            }
        });

    cubos.system("Check for jetpack pickup")
        .call([](Query<Player&, const CollidingWith&, const Jetpack&> collisions) {

            for (auto [player, collidingWith, jetpack] : collisions)
            {
                CUBOS_INFO("Jetpack picked up");
                player.jetpacked = true;
                player.jetpackTimer = 15.0F;
                CUBOS_INFO("Jetpacked = {}", player.jetpacked);
            }
        });

    cubos.system("Subtract jetpack timer")
        .call([](Query<Player&> players, const DeltaTime& dt) {

            for (auto [player] : players)
            {
                // Diminui o tempo do jetpack
                if (player.jetpacked && player.jetpackTimer > 0.0F) {
                    player.jetpackTimer -= dt.value();
                } else if (player.jetpacked) {
                    player.jetpacked = false;
                }

            }
        });

    cubos.system("Gradually speedup game")
        .call([](Query<Obstacle&> obstacles, const GameTime& gt) {

            for (auto [o] : obstacles) {

                o.velocity.z =  -(5.0F * gt.elapsedTime) - 80;
            }
        });

    cubos.run();
}
