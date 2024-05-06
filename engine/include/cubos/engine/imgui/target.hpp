/// @file
/// @brief Component @ref cubos::engine::ImGuiTarget.
/// @ingroup imgui-plugin

#pragma once

#include <cubos/core/gl/render_device.hpp>
#include <cubos/core/reflection/reflect.hpp>

#include <cubos/engine/api.hpp>

namespace cubos::engine
{
    /// @brief Component used to indicate that the ImGui frame should be rendered to a given @ref RenderTarget.
    /// @ingroup imgui-plugin
    struct CUBOS_ENGINE_API ImGuiTarget
    {
        CUBOS_REFLECT;

        /// @brief Framebuffer used to draw to the front @ref RenderPicker texture.
        core::gl::Framebuffer frontFramebuffer;

        /// @brief Framebuffer used to draw to the back @ref RenderPicker texture.
        core::gl::Framebuffer backFramebuffer;

        /// @brief Picker texture present in the @ref frontFramebuffer.
        core::gl::Texture2D frontPicker;

        /// @brief Picker texture present in the @ref backFramebuffer.
        core::gl::Texture2D backPicker;
    };
} // namespace cubos::engine
