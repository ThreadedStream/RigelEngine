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

#include "map_renderer.hpp"

#include "base/math_tools.hpp"
#include "data/game_traits.hpp"
#include "data/unit_conversions.hpp"

#include <cfenv>
#include <iostream>


namespace rigel::engine
{

using namespace data;

using data::map::BackdropScrollMode;


namespace
{

const auto ANIM_STATES = 4;
const auto FAST_ANIM_FRAME_DELAY = 1;
const auto SLOW_ANIM_FRAME_DELAY = 2;
const auto PARALLAX_FACTOR = 4;
const auto AUTO_SCROLL_PX_PER_SECOND_HORIZONTAL = 30;
const auto AUTO_SCROLL_PX_PER_SECOND_VERTICAL = 60;


base::Vector backdropOffset(
  const base::Vector& cameraPosition,
  const BackdropScrollMode scrollMode,
  const double backdropAutoScrollOffset)
{
  const auto parallaxBoth = scrollMode == BackdropScrollMode::ParallaxBoth;
  const auto parallaxHorizontal =
    scrollMode == BackdropScrollMode::ParallaxHorizontal || parallaxBoth;

  const auto autoScrollX = scrollMode == BackdropScrollMode::AutoHorizontal;
  const auto autoScrollY = scrollMode == BackdropScrollMode::AutoVertical;

  if (parallaxHorizontal || parallaxBoth)
  {
    return {
      parallaxHorizontal ? cameraPosition.x * PARALLAX_FACTOR : 0,
      parallaxBoth ? cameraPosition.y * PARALLAX_FACTOR : 0};
  }
  else if (autoScrollX || autoScrollY)
  {
    std::fesetround(FE_TONEAREST);
    const auto offsetPixels = base::round(backdropAutoScrollOffset);

    if (autoScrollX)
    {
      return {offsetPixels, 0};
    }
    else
    {
      return {0, GameTraits::viewPortHeightPx - offsetPixels};
    }
  }

  return {};
}


float speedForScrollMode(const BackdropScrollMode mode)
{
  if (mode == BackdropScrollMode::AutoHorizontal)
  {
    return AUTO_SCROLL_PX_PER_SECOND_HORIZONTAL;
  }
  else if (mode == BackdropScrollMode::AutoVertical)
  {
    return AUTO_SCROLL_PX_PER_SECOND_VERTICAL;
  }
  else
  {
    return 0.0f;
  }
}


float maxOffsetForScrollMode(const BackdropScrollMode mode)
{
  if (mode == BackdropScrollMode::AutoHorizontal)
  {
    return data::GameTraits::viewPortWidthPx;
  }
  else if (mode == BackdropScrollMode::AutoVertical)
  {
    return data::GameTraits::viewPortHeightPx;
  }
  else
  {
    return 1.0f;
  }
}


constexpr auto TILE_SET_IMAGE_LOGICAL_SIZE = base::Extents{
  tilesToPixels(data::GameTraits::CZone::tileSetImageWidth),
  tilesToPixels(data::GameTraits::CZone::tileSetImageHeight)};

} // namespace


MapRenderer::MapRenderer(
  renderer::Renderer* pRenderer,
  const data::map::Map* pMap,
  MapRenderData&& renderData)
  : mpRenderer(pRenderer)
  , mpMap(pMap)
  , mTileSetTexture(
      renderer::Texture(pRenderer, renderData.mTileSetImage),
      TILE_SET_IMAGE_LOGICAL_SIZE,
      pRenderer)
  , mBackdropTexture(mpRenderer, renderData.mBackdropImage)
  , mScrollMode(renderData.mBackdropScrollMode)
{
  if (renderData.mSecondaryBackdropImage)
  {
    mAlternativeBackdropTexture =
      renderer::Texture(mpRenderer, *renderData.mSecondaryBackdropImage);
  }
}


bool MapRenderer::hasHighResReplacements() const
{
  return mBackdropTexture.width() > data::GameTraits::viewPortWidthPx ||
    mBackdropTexture.height() > data::GameTraits::viewPortHeightPx ||
    mTileSetTexture.isHighRes();
}


void MapRenderer::switchBackdrops()
{
  std::swap(mBackdropTexture, mAlternativeBackdropTexture);
}


void MapRenderer::renderBackground(
  const base::Vector& sectionStart,
  const base::Extents& sectionSize) const
{
  renderMapTiles(sectionStart, sectionSize, DrawMode::Background);
}


void MapRenderer::renderForeground(
  const base::Vector& sectionStart,
  const base::Extents& sectionSize) const
{
  renderMapTiles(sectionStart, sectionSize, DrawMode::Foreground);
}


renderer::TexCoords MapRenderer::calculateBackdropTexCoords(
  const base::Vector& cameraPosition,
  const base::Extents& viewPortSize) const
{
  const auto offset =
    backdropOffset(cameraPosition, mScrollMode, mBackdropAutoScrollOffset);

  const auto offsetX = offset.x / float(GameTraits::viewPortWidthPx);
  const auto offsetY = offset.y / float(GameTraits::viewPortHeightPx);

  const auto windowWidth = float(mpRenderer->currentRenderTargetSize().width);
  const auto windowHeight = float(mpRenderer->currentRenderTargetSize().height);

  const auto scaleY = windowHeight / mBackdropTexture.height();

  const auto isOriginalSize =
    mBackdropTexture.width() == GameTraits::viewPortWidthPx &&
    mBackdropTexture.height() == GameTraits::viewPortHeightPx;

  const auto needsAspectRatioCorrection =
    isOriginalSize && windowHeight != GameTraits::viewPortHeightPx;
  const auto correctionFactor = needsAspectRatioCorrection ? 1.0f / 1.2f : 1.0f;
  const auto scaledWidth = scaleY * mBackdropTexture.width() * correctionFactor;
  const auto remappingFactor = windowWidth / scaledWidth;

  const auto targetWidth = float(data::tilesToPixels(viewPortSize.width)) *
    mpRenderer->globalScale().x;
  const auto targetHeight = float(data::tilesToPixels(viewPortSize.height)) *
    mpRenderer->globalScale().y;
  const auto visibleTargetPortionX = targetWidth / windowWidth;
  const auto visibleTargetPortionY = targetHeight / windowHeight;

  const auto left = offsetX;
  const auto top = offsetY;
  const auto right = left + visibleTargetPortionX * remappingFactor;
  const auto bottom = top + visibleTargetPortionY;

  return renderer::TexCoords{left, top, right, bottom};
}


void MapRenderer::renderBackdrop(
  const base::Vector& cameraPosition,
  const base::Extents& viewPortSize) const
{
  const auto saved = renderer::saveState(mpRenderer);
  mpRenderer->setTextureRepeatEnabled(true);
  mpRenderer->drawTexture(
    mBackdropTexture.data(),
    calculateBackdropTexCoords(cameraPosition, viewPortSize),
    {{}, data::tileExtentsToPixelExtents(viewPortSize)});
}


void MapRenderer::renderMapTiles(
  const base::Vector& sectionStart,
  const base::Extents& sectionSize,
  const DrawMode drawMode) const
{
  for (int layer = 0; layer < 2; ++layer)
  {
    for (int y = 0; y < sectionSize.height; ++y)
    {
      for (int x = 0; x < sectionSize.width; ++x)
      {
        const auto col = x + sectionStart.x;
        const auto row = y + sectionStart.y;
        if (col >= mpMap->width() || row >= mpMap->height())
        {
          continue;
        }

        const auto tileIndex = mpMap->tileAt(layer, col, row);
        const auto isForeground =
          mpMap->attributeDict().attributes(tileIndex).isForeGround();
        const auto shouldRenderForeground = drawMode == DrawMode::Foreground;

        if (isForeground != shouldRenderForeground)
        {
          continue;
        }

        renderTile(tileIndex, x, y);
      }
    }
  }
}


void MapRenderer::updateAnimatedMapTiles()
{
  ++mElapsedFrames;
}


void MapRenderer::updateBackdropAutoScrolling(const engine::TimeDelta dt)
{
  mBackdropAutoScrollOffset += dt * speedForScrollMode(mScrollMode);
  mBackdropAutoScrollOffset =
    std::fmod(mBackdropAutoScrollOffset, maxOffsetForScrollMode(mScrollMode));
}


void MapRenderer::renderSingleTile(
  const data::map::TileIndex index,
  const base::Vector& position,
  const base::Vector& cameraPosition) const
{
  const auto screenPosition = position - cameraPosition;
  renderTile(index, screenPosition.x, screenPosition.y);
}


void MapRenderer::renderTile(
  const data::map::TileIndex tileIndex,
  const int x,
  const int y) const
{
  // Tile index 0 is used to represent a transparent tile, i.e. the backdrop
  // should be visible. Therefore, don't draw if the index is 0.
  if (tileIndex != 0)
  {
    const auto tileIndexToDraw = animatedTileIndex(tileIndex);
    mTileSetTexture.renderTile(tileIndexToDraw, x, y);
  }
}


map::TileIndex
  MapRenderer::animatedTileIndex(const map::TileIndex tileIndex) const
{
  if (mpMap->attributeDict().attributes(tileIndex).isAnimated())
  {
    const auto fastAnimOffset =
      (mElapsedFrames / FAST_ANIM_FRAME_DELAY) % ANIM_STATES;
    const auto slowAnimOffset =
      (mElapsedFrames / SLOW_ANIM_FRAME_DELAY) % ANIM_STATES;

    const auto isFastAnim =
      mpMap->attributeDict().attributes(tileIndex).isFastAnimation();
    return tileIndex + (isFastAnim ? fastAnimOffset : slowAnimOffset);
  }
  else
  {
    return tileIndex;
  }
}

} // namespace rigel::engine
