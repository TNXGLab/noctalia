#include "shell/switcher/window_switcher_tile.h"

#include "capture/screencopy_capture.h"
#include "render/core/texture_manager.h"
#include "ui/builders.h"
#include "ui/palette.h"
#include "ui/style.h"

#include <algorithm>
#include <cmath>

namespace {

  constexpr float kPreviewAspect = 16.0F / 9.0F;
  constexpr float kFallbackIconScale = 0.38F;
  constexpr float kHeaderHeight = 30.0F;
  constexpr float kHeaderIconSize = 16.0F;

  [[nodiscard]] float closeHitSize(float contentScale) { return Style::controlHeightSm * contentScale * 0.72F; }

  [[nodiscard]] float framePadding(float contentScale) { return Style::spaceXs * contentScale; }

  [[nodiscard]] float headerHeight(float contentScale) { return kHeaderHeight * contentScale; }

  [[nodiscard]] float headerIconSize(float contentScale) { return kHeaderIconSize * contentScale; }

  [[nodiscard]] std::pair<float, float>
  iconHostDimensions(float cellWidth, float cellHeight, float contentScale) noexcept {
    const float framePad = framePadding(contentScale);
    const float innerGap = Style::spaceXs * contentScale;
    const float iconHostWidth = std::max(0.0F, cellWidth - framePad * 2.0F);
    const float iconHostHeight = std::max(
        72.0F,
        std::min(
            iconHostWidth / kPreviewAspect,
            std::max(0.0F, cellHeight - framePad * 2.0F - innerGap - headerHeight(contentScale))
        )
    );
    return {iconHostWidth, iconHostHeight};
  }

  void applyCloseGlyphStyle(Glyph* glyph, const ColorSpec& fill, float contentScale) {
    if (glyph == nullptr) {
      return;
    }
    glyph->setColor(fill);
    const float offset = std::max(0.5F, 0.85F * contentScale);
    glyph->setShadow(colorSpecFromRole(ColorRole::Shadow, 0.55F), 0.0F, offset);
  }

} // namespace

bool WindowSwitcherTile::hitTestCloseRegion(
    float cellWidth, float /*cellHeight*/, float contentScale, float localX, float localY
) noexcept {
  const float framePad = framePadding(contentScale);
  const float hitSize = closeHitSize(contentScale);
  const float inset = Style::spaceXs * contentScale * 0.55F;
  const float x = cellWidth - framePad - hitSize - inset;
  const float y = framePad + std::max(0.0F, (headerHeight(contentScale) - hitSize) * 0.5F);
  return localX >= x && localX < x + hitSize && localY >= y && localY < y + hitSize;
}

WindowSwitcherTile::WindowSwitcherTile(float contentScale, AsyncTextureCache* asyncTextures)
    : m_contentScale(contentScale), m_asyncTextures(asyncTextures) {
  setHitTestVisible(false);

  const float frameRadius = Style::scaledRadiusLg(m_contentScale);
  const float previewRadius = Style::scaledRadiusMd(m_contentScale);
  const float closeBackdropSize = closeHitSize(m_contentScale) + Style::spaceXs * m_contentScale * 0.45F;
  const float smallIconSize = headerIconSize(m_contentScale);

  auto layout = ui::column({
      .out = &m_layout,
      .align = FlexAlign::Stretch,
  });
  addChild(std::move(layout));

  m_layout->addChild(
      ui::box({
          .out = &m_frame,
          .fill = colorSpecFromRole(ColorRole::Surface, 0.56F),
          .radius = frameRadius,
          .configure = [frameRadius](Box& box) {
            box.setRadius(frameRadius);
            box.setClipChildren(true);
          },
      })
  );

  m_frame->addChild(
      ui::column({
          .out = &m_inner,
          .align = FlexAlign::Stretch,
          .gap = Style::spaceXs * m_contentScale,
          .configure = [this, previewRadius, smallIconSize](Flex& column) {
            column.addChild(
                ui::row({
                    .out = &m_caption,
                    .align = FlexAlign::Center,
                    .gap = Style::spaceXs * m_contentScale * 1.25F,
                    .configure = [this](Flex& header) { header.setMinHeight(headerHeight(m_contentScale)); },
                })
            );
            m_caption->addChild(
                ui::image({
                    .out = &m_headerIcon,
                    .fit = ImageFit::Contain,
                    .width = smallIconSize,
                    .height = smallIconSize,
                    .visible = false,
                })
            );
            m_caption->addChild(
                ui::label({
                    .out = &m_title,
                    .fontSize = Style::fontSizeCaption * m_contentScale,
                    .fontWeight = FontWeight::Medium,
                    .color = colorSpecFromRole(ColorRole::OnSurface),
                    .maxLines = 1,
                    .ellipsize = TextEllipsize::End,
                    .flexGrow = 1.0F,
                    .configure = [](Label& label) { label.setTextAlign(TextAlign::Start); },
                })
            );

            column.addChild(
                ui::box({
                    .out = &m_iconHost,
                    .fill = colorSpecFromRole(ColorRole::SurfaceVariant, 0.72F),
                    .radius = previewRadius,
                    .configure = [previewRadius](Box& box) {
                      box.setRadius(previewRadius);
                      box.setClipChildren(true);
                      box.setBorder(colorSpecFromRole(ColorRole::Outline, 0.42F), Style::borderWidth);
                    },
                })
            );
          },
      })
  );

  m_frame->addChild(
      ui::box({
          .out = &m_closeBackdrop,
          .fill = colorSpecFromRole(ColorRole::Surface, 0.92F),
          .radius = closeBackdropSize * 0.5F,
          .visible = false,
          .participatesInLayout = false,
      })
  );
  m_closeBackdrop->setZIndex(30);

  m_frame->addChild(
      ui::glyph({
          .out = &m_closeGlyph,
          .glyph = "close",
          .glyphSize = Style::fontSizeCaption * m_contentScale * 0.92F,
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
          .visible = false,
          .participatesInLayout = false,
      })
  );
  m_closeGlyph->setZIndex(31);

  // 预览保留完整窗口比例；不同长宽比由底色留边，而不是裁掉窗口内容。
  m_iconHost->addChild(
      ui::image({
          .out = &m_preview,
          .fit = ImageFit::Contain,
          .visible = false,
          .participatesInLayout = false,
      })
  );
  m_iconHost->addChild(
      ui::image({
          .out = &m_icon,
          .fit = ImageFit::Contain,
          .visible = false,
          .participatesInLayout = false,
      })
  );
  m_iconHost->addChild(
      ui::glyph({
          .out = &m_fallbackGlyph,
          .glyph = "app-window",
          .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
          .visible = true,
          .participatesInLayout = false,
      })
  );

  m_icon->setAsyncReadyCallback([this]() {
    if (m_icon == nullptr || m_fallbackGlyph == nullptr || !m_icon->hasImage()) {
      return;
    }
    const bool previewVisible = m_previewRevision != 0 && m_preview != nullptr && m_preview->visible();
    m_icon->setVisible(!previewVisible);
    m_fallbackGlyph->setVisible(false);
    markLayoutDirty();
    if (m_onInvalidate) {
      m_onInvalidate();
    }
  });
  m_headerIcon->setAsyncReadyCallback([this]() {
    if (m_headerIcon == nullptr || !m_headerIcon->hasImage()) {
      return;
    }
    m_headerIcon->setVisible(true);
    markLayoutDirty();
    if (m_onInvalidate) {
      m_onInvalidate();
    }
  });

  auto focusShadow = ui::box({
      .out = &m_focusShadow,
      .fill = clearColorSpec(),
      .radius = frameRadius + Style::focusRingWidth,
      .visible = false,
      .participatesInLayout = false,
      .configure = [](Box& box) {
        box.setBorder(colorSpecFromRole(ColorRole::Shadow, 0.82F), Style::focusRingWidth + 3.0F);
      },
  });
  focusShadow->setHitTestVisible(false);
  focusShadow->setZIndex(20);
  addChild(std::move(focusShadow));

  auto focusRing = ui::box({
      .out = &m_focusRing,
      .fill = clearColorSpec(),
      .radius = frameRadius,
      .visible = false,
      .participatesInLayout = false,
  });
  focusRing->setHitTestVisible(false);
  focusRing->setZIndex(21);
  addChild(std::move(focusRing));
}

void WindowSwitcherTile::setCellSize(float cellWidth, float cellHeight) {
  m_cellWidth = std::max(0.0F, cellWidth);
  m_cellHeight = std::max(0.0F, cellHeight);
  setSize(m_cellWidth, m_cellHeight);

  const float framePad = framePadding(m_contentScale);
  const float innerGap = Style::spaceXs * m_contentScale;
  const float closeReserve = closeHitSize(m_contentScale) + Style::spaceSm * m_contentScale;
  std::tie(m_iconHostWidth, m_iconHostHeight) =
      iconHostDimensions(m_cellWidth, m_cellHeight, m_contentScale);

  if (m_layout != nullptr) {
    m_layout->setFrameSize(m_cellWidth, m_cellHeight);
  }
  if (m_frame != nullptr) {
    m_frame->setFrameSize(m_cellWidth, m_cellHeight);
  }
  if (m_inner != nullptr) {
    m_inner->setPadding(framePad);
    m_inner->setGap(innerGap);
    m_inner->setFrameSize(m_cellWidth, m_cellHeight);
  }
  if (m_caption != nullptr) {
    m_caption->setPadding(0.0F, closeReserve, 0.0F, 0.0F);
    m_caption->setMinHeight(headerHeight(m_contentScale));
  }
  if (m_iconHost != nullptr) {
    m_iconHost->setFrameSize(m_iconHostWidth, m_iconHostHeight);
  }
  if (m_preview != nullptr) {
    m_preview->setPosition(0.0F, 0.0F);
    m_preview->setSize(m_iconHostWidth, m_iconHostHeight);
  }
  if (m_title != nullptr) {
    const float reservedWidth =
        headerIconSize(m_contentScale) + Style::spaceXs * m_contentScale * 1.25F + closeReserve;
    m_title->setMaxWidth(std::max(0.0F, m_iconHostWidth - reservedWidth));
  }
  if (m_focusShadow != nullptr) {
    m_focusShadow->setPosition(0.0F, 0.0F);
    m_focusShadow->setSize(m_cellWidth, m_cellHeight);
  }
  if (m_focusRing != nullptr) {
    m_focusRing->setPosition(0.0F, 0.0F);
    m_focusRing->setSize(m_cellWidth, m_cellHeight);
  }

  markLayoutDirty();
}

void WindowSwitcherTile::setCloseHovered(bool hovered) {
  if (m_closeHovered == hovered) {
    return;
  }
  m_closeHovered = hovered;
  applyCloseVisualState();
}

void WindowSwitcherTile::bind(
    Renderer& renderer, const WindowSwitcherEntry& entry, bool selected, bool hovered, const ScreencopyImage* preview,
    std::uint64_t previewRevision
) {
  m_entry = entry;
  m_hasEntry = true;
  m_selected = selected;
  m_hovered = hovered;

  m_title->setText(entry.title.empty() ? entry.appLabel : entry.title);

  if (entry.iconPath != m_iconPath) {
    m_iconPath = entry.iconPath;
    m_iconTargetSize = 0;
    m_headerIconTargetSize = 0;
    m_icon->clear(renderer);
    m_headerIcon->clear(renderer);
    m_headerIcon->setVisible(false);
  }

  applyPreview(renderer, preview, previewRevision);
  applyVisualState();
  applyCloseVisualState();
  markLayoutDirty();
}

void WindowSwitcherTile::applyPreview(
    Renderer& renderer, const ScreencopyImage* preview, std::uint64_t previewRevision
) {
  const bool hasPreview =
      preview != nullptr && !preview->rgba.empty() && preview->width > 0 && preview->height > 0;

  if (!hasPreview) {
    if (m_previewRevision != 0) {
      m_preview->clear(renderer);
      m_previewRevision = 0;
    }
    m_preview->setVisible(false);
    return;
  }

  // 缓存对象地址保持不变；后台采集就地替换像素后，修订号确保纹理仅重新上传一次。
  if (previewRevision != m_previewRevision) {
    const bool ready = m_preview->setSourceRaw(
        renderer, preview->rgba.data(), preview->rgba.size(), preview->width, preview->height, preview->width * 4,
        PixmapFormat::RGBA, false
    );
    if (!ready) {
      m_preview->setVisible(false);
      m_previewRevision = 0;
      return;
    }
    m_previewRevision = previewRevision;
  }
  m_preview->setVisible(true);
  m_icon->setVisible(false);
  m_fallbackGlyph->setVisible(false);
}

bool WindowSwitcherTile::refreshIcon(Renderer& renderer) {
  const bool previewVisible = m_previewRevision != 0 && m_preview != nullptr && m_preview->visible();
  if (!m_hasEntry || m_iconPath.empty()) {
    m_headerIcon->setVisible(false);
    m_icon->setVisible(false);
    m_fallbackGlyph->setVisible(!previewVisible);
    return previewVisible;
  }

  m_icon->setAppIconColorization(m_appIconColorizeTint);
  m_headerIcon->setAppIconColorization(m_appIconColorizeTint);

  bool iconReady = false;
  bool headerIconReady = false;
  if (m_asyncTextures != nullptr) {
    iconReady = m_icon->setSourceFileAsync(renderer, *m_asyncTextures, m_iconPath, m_iconTargetSize, true);
    headerIconReady =
        m_headerIcon->setSourceFileAsync(renderer, *m_asyncTextures, m_iconPath, m_headerIconTargetSize, true);
  } else {
    iconReady = m_icon->setSourceFile(renderer, m_iconPath, m_iconTargetSize, true);
    headerIconReady = m_headerIcon->setSourceFile(renderer, m_iconPath, m_headerIconTargetSize, true);
  }

  const float fallbackIconSize = std::min(m_iconHostWidth, m_iconHostHeight) * kFallbackIconScale;
  m_icon->setSize(fallbackIconSize, fallbackIconSize);
  m_headerIcon->setSize(headerIconSize(m_contentScale), headerIconSize(m_contentScale));
  m_headerIcon->setVisible(headerIconReady);
  m_icon->setVisible(!previewVisible && iconReady);
  m_fallbackGlyph->setVisible(!previewVisible && !iconReady);
  return previewVisible || iconReady || headerIconReady;
}

void WindowSwitcherTile::applyVisualState() {
  const float frameRadius = Style::scaledRadiusLg(m_contentScale);
  if (m_selected) {
    m_frame->setFill(colorSpecFromRole(ColorRole::Surface, 0.92F));
    m_frame->setBorder(colorSpecFromRole(ColorRole::Outline, 0.78F), Style::borderWidth);
  } else if (m_hovered) {
    m_frame->setFill(colorSpecFromRole(ColorRole::Surface, 0.74F));
    m_frame->setBorder(colorSpecFromRole(ColorRole::Hover, 0.82F), Style::borderWidth);
  } else {
    m_frame->setFill(colorSpecFromRole(ColorRole::Surface, 0.56F));
    m_frame->setBorder(colorSpecFromRole(ColorRole::Outline, 0.52F), Style::borderWidth);
  }
  m_frame->setRadius(frameRadius);
  m_frame->setOpacity(1.0F);

  if (m_focusShadow != nullptr) {
    m_focusShadow->setVisible(m_selected);
  }
  if (m_focusRing != nullptr) {
    m_focusRing->setVisible(m_selected || m_hovered);
    m_focusRing->setBorder(
        colorSpecFromRole(m_selected ? ColorRole::Primary : ColorRole::Hover, m_selected ? 1.0F : 0.84F),
        m_selected ? Style::focusRingWidth : Style::borderWidth
    );
  }
}

void WindowSwitcherTile::applyCloseVisualState() {
  if (m_closeGlyph == nullptr) {
    return;
  }
  const bool showClose = m_selected || m_hovered || m_closeHovered;
  m_closeGlyph->setVisible(showClose);
  if (m_closeBackdrop != nullptr) {
    m_closeBackdrop->setVisible(m_closeHovered);
  }
  if (m_closeHovered) {
    applyCloseGlyphStyle(m_closeGlyph, colorSpecFromRole(ColorRole::Error), m_contentScale);
  } else {
    applyCloseGlyphStyle(m_closeGlyph, colorSpecFromRole(ColorRole::OnSurfaceVariant, 0.88F), m_contentScale);
  }
}

void WindowSwitcherTile::layoutOverlays(Renderer& renderer) {
  if (m_iconHostWidth <= 0.0F || m_iconHostHeight <= 0.0F) {
    return;
  }

  const float framePad = framePadding(m_contentScale);
  const float hitSize = closeHitSize(m_contentScale);
  const float inset = Style::spaceXs * m_contentScale * 0.55F;
  const float backdropSize = hitSize + Style::spaceXs * m_contentScale * 0.45F;
  const float closeX = std::round(m_cellWidth - framePad - hitSize - inset);
  const float closeY =
      std::round(framePad + std::max(0.0F, (headerHeight(m_contentScale) - hitSize) * 0.5F));

  if (m_closeBackdrop != nullptr) {
    m_closeBackdrop->setPosition(
        std::round(closeX - (backdropSize - hitSize) * 0.5F),
        std::round(closeY - (backdropSize - hitSize) * 0.5F)
    );
    m_closeBackdrop->setSize(backdropSize, backdropSize);
  }
  if (m_closeGlyph != nullptr) {
    m_closeGlyph->measure(renderer);
    const float glyphW = m_closeGlyph->width() > 0.0F ? m_closeGlyph->width() : hitSize;
    const float glyphH = m_closeGlyph->height() > 0.0F ? m_closeGlyph->height() : hitSize;
    m_closeGlyph->setPosition(
        std::round(closeX + (hitSize - glyphW) * 0.5F), std::round(closeY + (hitSize - glyphH) * 0.5F)
    );
  }

  const int iconTarget =
      std::max(32, static_cast<int>(std::round(std::min(m_iconHostWidth, m_iconHostHeight) * kFallbackIconScale)));
  const int headerIconTarget =
      std::max(16, static_cast<int>(std::round(headerIconSize(m_contentScale) * 2.0F)));
  m_iconTargetSize = iconTarget;
  m_headerIconTargetSize = headerIconTarget;
  if (m_hasEntry && !m_iconPath.empty()) {
    (void)refreshIcon(renderer);
  }

  const float fallbackIconSize = std::min(m_iconHostWidth, m_iconHostHeight) * kFallbackIconScale;
  if (m_icon != nullptr && m_icon->visible()) {
    m_icon->setPosition(
        std::round((m_iconHostWidth - m_icon->width()) * 0.5F),
        std::round((m_iconHostHeight - m_icon->height()) * 0.5F)
    );
  }
  if (m_fallbackGlyph != nullptr && m_fallbackGlyph->visible()) {
    m_fallbackGlyph->setGlyphSize(fallbackIconSize);
    m_fallbackGlyph->measure(renderer);
    m_fallbackGlyph->setPosition(
        std::round((m_iconHostWidth - m_fallbackGlyph->width()) * 0.5F),
        std::round((m_iconHostHeight - m_fallbackGlyph->height()) * 0.5F)
    );
  }
}

void WindowSwitcherTile::doLayout(Renderer& renderer) {
  if (m_cellWidth > 0.0F && m_cellHeight > 0.0F && (m_cellWidth != width() || m_cellHeight != height())) {
    setCellSize(width(), height());
  }

  InputArea::doLayout(renderer);
  layoutOverlays(renderer);
}
