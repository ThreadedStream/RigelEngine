/* Copyright (C) 2016, Nikolai Wuttke. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/color.hpp"
#include "base/defer.hpp"
#include "base/spatial_types.hpp"
#include "base/warnings.hpp"
#include "data/image.hpp"

RIGEL_DISABLE_WARNINGS
#include <SDL_video.h>
RIGEL_RESTORE_WARNINGS

#include <cstdint>
#include <memory>
#include <optional>


namespace rigel::renderer
{

using TextureId = std::uint32_t;

/** Texture coordinates for Renderer::drawTexture()
 *
 * Values should be in range [0.0, 1.0] - unless texture repeat is
 * enabled. Use the toTexCoords() helper function to create these from
 * a source rectangle.
 */
struct TexCoords
{
  float left;
  float top;
  float right;
  float bottom;
};


/** Convert a source rect to texture coordinates
 *
 * Renderer::drawTexture() expects normalized texture coordinates,
 * but most of the time, it's easier to work with image-specific
 * coordinates, like e.g. "from 8,8 to 32,64". This helper function
 * converts from the latter to the former.
 */
inline TexCoords toTexCoords(
  const base::Rect<int>& sourceRect,
  const int texWidth,
  const int texHeight)
{
  const auto left = sourceRect.topLeft.x / float(texWidth);
  const auto top = sourceRect.topLeft.y / float(texHeight);
  const auto width = sourceRect.size.width / float(texWidth);
  const auto height = sourceRect.size.height / float(texHeight);
  const auto right = left + width;
  const auto bottom = top + height;

  return {left, top, right, bottom};
}


/** OpenGL-based 2D rendering API
 *
 * This class provides hardware-accelerated 2D rendering capabilities
 * on top of OpenGL 3.0 or OpenGL ES 2.0. It supports implicit
 * batching of draw calls and handles OpenGL state changes
 * efficiently.
 * Render targets for rendering to textures, global transformations
 * (scaling, translation), and a few color effects are also available.
 *
 * A valid OpenGL context must be created before instantiating this
 * class.
 */
class Renderer
{
public:
  explicit Renderer(SDL_Window* pWindow);
  ~Renderer();

  // Drawing API
  ////////////////////////////////////////////////////////////////////////

  /** Draw (part of) texture into given rectangle
   *
   * This is a low-level API. Using the renderer::Texture class instead is
   * recommended for most use cases.
   *
   * Renders a texture that has been created via createTexture() at the
   * given destination rectangle, using the part of the texture
   * identified by the source rectangle.
   * If the destination rectangle has different dimensions than the
   * source rectangle, the image will be scaled accordingly.
   * The destination rectangle's coordinates are modified by the current
   * global scale and translation. The texture's pixels are modified by
   * the current overlay color and color modulation.
   * If texture repeat is enabled, a source rect that's larger than the
   * texture itself will cause the texture to be rendered multiple times.
   *
   * Supports batching: Multiple calls to this function will be combined
   * into a single vertex buffer and OpenGL draw call, as long as the
   * same texture is used. Changing any state will also interrupt the
   * current batch.
   * For best efficiency, consider using a renderer::TextureAtlas to
   * combine multiple images into a single texture.
   */
  void drawTexture(
    TextureId texture,
    const TexCoords& sourceRect,
    const base::Rect<int>& destRect);

  /** Draw single pixel
   *
   * Supports batching: Multiple calls to this function will be combined
   * into a single vertex buffer and OpenGL draw call.
   * Changing any state will interrupt the current batch.
   *
   * Position is modified by the current global scale and translation.
   * Color modulation and overlay color are ignored.
   */
  void drawPoint(const base::Vector& position, const base::Color& color);

  /** Draw "under water" effect
   *
   * Contrary to the other functions offered by the renderer, this one
   * is very specific to Duke Nukem II. It draws the given texture with
   * a special shader modifying all colors to be shades of blue.
   * The area rectangle is used as both source and target rectangle, as
   * the texture typically represents a rendered game scene.
   * If an animation step is given, the top-most pixels of the given
   * area will appear in one of 4 possible wave patterns. Otherwise,
   * the entire area is drawn uniformly. The animation step must be a
   * number between 0 and 3.
   *
   * Supports batching: Multiple calls to this function will be combined
   * into a single vertex buffer and OpenGL draw call, as long as the
   * same texture is used. Changing any state will also interrupt the
   * current batch.
   */
  void drawWaterEffect(
    const base::Rect<int>& area,
    TextureId unprocessedScreen,
    std::optional<int> surfaceAnimationStep);

  /** Draw rectangle outline, 1 pixel wide
   *
   * _Warning_: Does not support batching, use sparingly or only for
   * debugging.
   *
   * Rectangle coordinates are modified by the current global scale
   * and translation.
   * Color modulation and overlay color are ignored.
   */
  void drawRectangle(const base::Rect<int>& rect, const base::Color& color);

  /** Draw filled rectangle
   *
   * _Warning_: Does not support batching, use sparingly or only for
   * debugging.
   *
   * Rectangle coordinates are modified by the current global scale
   * and translation.
   * Color modulation and overlay color are ignored.
   */
  void
    drawFilledRectangle(const base::Rect<int>& rect, const base::Color& color);

  /** Draw line, 1 pixel wide
   *
   * _Warning_: Does not support batching, use sparingly or only for
   * debugging.
   *
   * Coordinates are modified by the current global scale and
   * translation.
   * Color modulation and overlay color are ignored.
   */
  void drawLine(
    const base::Vector& start,
    const base::Vector& end,
    const base::Color& color)
  {
    drawLine(start.x, start.y, end.x, end.y, color);
  }

  /** Convenience overload for drawLine() */
  void drawLine(int x1, int y1, int x2, int y2, const base::Color& color);

  /** Fill screen/render target with solid color */
  void clear(const base::Color& clearColor = {0, 0, 0, 255});

  /** Display current frame on screen
   *
   * When V-Sync is enabled, blocks until the next vertical blank,
   * in order to synchronize rendering with the display's refresh rate.
   * Returns immediately otherwise.
   *
   * Use SDL_GL_SetSwapInterval to configure V-Sync.
   */
  void swapBuffers();

  /** Explicitly submit any pending draw calls to OpenGL
   *
   * This function forces submitting any currently queued up drawing
   * commands. It's not needed most of the time, because the renderer
   * already takes care of submitting whenever necessary. But in cases
   * where the renderer doesn't know that it should submit, for example
   * when combining it with independent rendering code like a UI
   * library, you can use it to explicitly trigger submission.
   */
  void submitBatch();

  // Resource management API
  ////////////////////////////////////////////////////////////////////////

  /** Create a texture
   *
   * This is a low-level API. Using the renderer::Texture class instead
   * is recommended for most use cases.
   *
   * Uploads the given image to the GPU to create a texture.
   * The returned texture id can then be used with drawTexture() to
   * draw the image on screen.
   *
   * When a texture is no longer needed, it must be destroyed using
   * destroyTexture().
   */
  TextureId createTexture(const data::Image& image);

  /** Create a render target texture
   *
   * This is a low-level API. Using the renderer::RenderTarget class
   * instead is recommended for most use cases.
   *
   * Like createTexture, but the resulting texture can be bound as a
   * render target using setRenderTarget().
   */
  TextureId createRenderTargetTexture(int width, int height);

  /** Destroy a previously created texture or render target
   *
   * This is a low-level API. Using the Texture and RenderTarget classes
   * instead is recommended for most use cases.
   */
  void destroyTexture(TextureId texture);

  void setFilteringEnabled(TextureId texture, bool enabled);

  // State management API
  ////////////////////////////////////////////////////////////////////////

  /** Snapshot current state for later restoration
   *
   * Saves all renderer state (color modifiers, texture repeat,
   * translation, scale, clip rect, and render target) into a snapshot.
   * Calling popState() reapplies the last saved state, effectively
   * undoing any state changes that happened between pushState() and
   * popState(). Calling this function does not change any renderer
   * state, nor does it interrupt the current batch.
   *
   * Calls to pushState() and popState() must be balanced, i.e. there
   * must be exactly one popState() for each pushState().
   * It's recommended to use the saveState() helper function, which
   * guarantees this.
   */
  void pushState();

  /** Restore last saved state snapshot
   *
   * See pushState() for more info.
   * Does only interrupt the current batch if the restored snapshot
   * is different from the current state.
   */
  void popState();

  /** Reset all state to default values */
  void resetState();

  /** Set color to overlay on top of texture colors
   *
   * Part of the renderer state. Used by drawTexture().
   * The overlay color is added to each pixel in the texture,
   * but the original pixel's alpha is preserved. This means
   * you can use it on images which are partially transparent,
   * like typical masked sprites, and only the visible pixels
   * will be affected.
   * Can be used to implement effects like white flashing to
   * indicate taking damage, etc. Default value is transparent
   * black (RGBA 0, 0, 0, 0) which has no visible effect.
   *
   * _Note_: Using a non-default overlay color causes a more
   * expensive shader to be used.
   */
  void setOverlayColor(const base::Color& color);

  /** Set color to multiply texture colors by
   *
   * Part of the renderer state. Used by drawTexture().
   * Eeach pixel in the texture is multiplied by the current
   * color modulation.
   * Can be used to "color in" a UI element or bitmap font in different
   * colors, for example (the source texture needs to be white for that).
   * Default value is white (RGBA 255, 255, 255, 255) which has no visible
   * effect, as it's essentially a multiplication by 1.
   *
   * _Note_: Using a non-default color modulation causes a more
   * expensive shader to be used.
   */
  void setColorModulation(const base::Color& colorModulation);

  /** Enable/disable texture repeat for drawTexture()
   *
   * Part of the renderer state. Used by drawTexture().
   * When enabled, texture coordinates that lie outside the source image
   * will cause the image to be drawn repeated multiple times.
   * This is the same as when using GL_REPEAT, but it's a render state
   * here instead of being a property of the texture.
   *
   * The reason for doing it this way is that OpenGL ES does not support
   * using GL_REPEAT for non-power of two textures, so we need to
   * implement repeating ourselves in the shader if we want to allow
   * repeating for non-power of two textures in the GL ES version.
   * For simplicity's sake, we always do it this way even when using
   * Desktop GL which doesn't have this limitation - keeping the code
   * the same between both versions simplifies the renderer.
   *
   * _Note_: Enabling repeat causes a more
   * expensive shader to be used.
   */
  void setTextureRepeatEnabled(bool enable);

  /** Set offset to be added to all coordinates before rendering
   *
   * Part of the renderer state.
   * All coordinates given to the drawing functions are modified
   * by the current global translation before rendering. The
   * addition is done in the shader, so there is no CPU overhead.
   * Translation is applied after global scaling (see
   * setGlobalScale()).
   *
   * Global translation is handy as it allows client code to operate
   * in a local coordinate system, e.g. a UI which always renders at
   * (0,0), and then control where on screen the result should be drawn
   * without modifying the code. E.g. drawing the same UI multiple
   * times at different locations.
   */
  void setGlobalTranslation(const base::Vector& translation);

  /** Set scale factor to be applied to all coordinates before rendering
   *
   * Part of the renderer state.
   * Similarly to global translation, this defines a transformation to
   * be applied to all coordinates that are given to the drawing
   * functions. In this case, coordinates are multiplied by the given
   * vector in the shader (no CPU overhead). Scaling is applied before
   * translation.
   *
   * Global scaling is useful for e.g. upscaling low resolution content
   * to native screen resolution. Together with global translation,
   * scaling defines a local coordinate system which makes it possible
   * to write client code as if the screen had a fixed resolution,
   * e.g. drawing into an area from (0,0) to (320,200), and then have
   * the resulting image be automatically stretched and offset to native
   * screen resolution, without needing the client code to be aware of
   * this. Also see upscaling_utils.hpp.
   */
  void setGlobalScale(const base::Point<float>& scale);

  /** Set clipping rectangle to constrain rendering
   *
   * Part of the renderer state.
   * When a clipping rectangle is set, any drawing operations are
   * constrained to the area specified by the given rectangle. In
   * other words, only pixels contained in the rectangle are actually
   * drawn, everything outside the rectangle remains unchanged.
   */
  void setClipRect(const std::optional<base::Rect<int>>& clipRect);

  /** Bind render target
   *
   * This is a low-level API. Using the renderer::RenderTarget class
   * instead is recommended for most use cases.
   *
   * Target must be a texture id that has been created via
   * createRenderTargetTexture(). Calling this function binds the
   * texture as render target, which means that any subsequent
   * drawing commands result in drawing to the texture instead of the
   * screen. Any previously bound render target is unbound.
   * A texture id of 0 binds the default render target, i.e. the
   * screen.
   */
  void setRenderTarget(TextureId target);

  base::Size<int> currentRenderTargetSize() const;
  base::Size<int> windowSize() const;

  base::Vector globalTranslation() const;
  base::Point<float> globalScale() const;
  std::optional<base::Rect<int>> clipRect() const;

private:
  struct Impl;
  std::unique_ptr<Impl> mpImpl;
};

/** RAII helper for temporarily saving state
 *
 * Use this to snapshot the current renderer state, and automatically
 * restore it when leaving the current scope.
 */
[[nodiscard]] inline auto saveState(Renderer* pRenderer)
{
  pRenderer->pushState();
  return base::defer([=]() { pRenderer->popState(); });
}

} // namespace rigel::renderer
