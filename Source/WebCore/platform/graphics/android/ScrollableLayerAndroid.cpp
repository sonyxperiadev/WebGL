#include "config.h"
#include "ScrollableLayerAndroid.h"

#include "GLWebViewState.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

bool ScrollableLayerAndroid::scrollTo(int x, int y)
{
    SkIRect scrollBounds;
    getScrollRect(&scrollBounds);
    if (!scrollBounds.fRight && !scrollBounds.fBottom)
        return false;

    SkScalar newX = SkScalarPin(x, 0, scrollBounds.fRight);
    SkScalar newY = SkScalarPin(y, 0, scrollBounds.fBottom);
    // Check for no change.
    if (newX == scrollBounds.fLeft && newY == scrollBounds.fTop)
        return false;

    setPosition(m_scrollLimits.fLeft - newX, m_scrollLimits.fTop - newY);

    return true;
}

void ScrollableLayerAndroid::getScrollRect(SkIRect* out) const
{
    const SkPoint& pos = getPosition();
    out->fLeft = m_scrollLimits.fLeft - pos.fX;
    out->fTop = m_scrollLimits.fTop - pos.fY;
    out->fRight = getSize().width() - m_scrollLimits.width();
    out->fBottom = getSize().height() - m_scrollLimits.height();
}

bool ScrollableLayerAndroid::scrollRectIntoView(const SkIRect& rect)
{
    // Apply the local transform to the rect to get it relative to the parent
    // layer.
    SkMatrix localTransform;
    getLocalTransform(&localTransform);
    SkRect transformedRect;
    transformedRect.set(rect);
    localTransform.mapRect(&transformedRect);

    // Test left before right to prioritize left alignment if transformedRect is wider than
    // visible area.
    int x = m_scrollLimits.fLeft;
    if (transformedRect.fLeft < m_scrollLimits.fLeft)
        x = transformedRect.fLeft;
    else if (transformedRect.fRight > m_scrollLimits.fRight)
        x = transformedRect.fRight - std::max(m_scrollLimits.width(), transformedRect.width());

    // Test top before bottom to prioritize top alignment if transformedRect is taller than
    // visible area.
    int y = m_scrollLimits.fTop;
    if (transformedRect.fTop < m_scrollLimits.fTop)
        y = transformedRect.fTop;
    else if (transformedRect.fBottom > m_scrollLimits.fBottom)
        y = transformedRect.fBottom - std::max(m_scrollLimits.height(), transformedRect.height());

    return scrollTo(x - getPosition().fX, y - getPosition().fY);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
