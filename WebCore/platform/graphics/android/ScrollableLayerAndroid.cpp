#include "config.h"
#include "ScrollableLayerAndroid.h"

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

    SkScalar diffX = newX - scrollBounds.fLeft;
    SkScalar diffY = newY - scrollBounds.fTop;
    const SkPoint& pos = getPosition();
    setPosition(pos.fX - diffX, pos.fY - diffY);
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

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
