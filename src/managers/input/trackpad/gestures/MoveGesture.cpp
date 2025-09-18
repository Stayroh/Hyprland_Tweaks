#include "MoveGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"

void CMoveTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_window    = g_pCompositor->m_lastWindow;
    m_lastDelta = {};
}

void CMoveTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;
    const auto DELTA = e.swipe ? e.swipe->delta : e.pinch->delta;
    if (m_window->m_isFloating) {
        g_pLayoutManager->getCurrentLayout()->moveActiveWindow(DELTA, m_window.lock());
        m_window->m_realSize->warp();
        m_window->m_realPosition->warp();
        return;
    }
    // tiled window -> displace, then execute a move dispatcher on end.
    g_pHyprRenderer->damageWindow(m_window.lock());
    // funny name but works on tiled too lmao
    m_lastDelta += DELTA;
    
    // Asymptotic function parameters
    const float MAX_OFFSET = 100.0F;
    const float SENSITIVITY = 0.005F; // Controls how quickly it approaches the limit
    
    Vector2D scaledDelta = m_lastDelta; // * 0.5F;
    Vector2D asymptotic_offset;
    
    // Calculate the magnitude of the scaled delta
    float magnitude = std::sqrt(scaledDelta.x * scaledDelta.x + scaledDelta.y * scaledDelta.y);
    
    if (magnitude > 0.001F) {
        // Apply exponential limiting to the magnitude
        float limited_magnitude = MAX_OFFSET * (1.0F - std::exp(-SENSITIVITY * magnitude)) / (1.0F - (1.0F - MAX_OFFSET * SENSITIVITY) * std::exp(-SENSITIVITY * magnitude));
        
        // Calculate the scale factor to maintain direction while limiting magnitude
        float scale_factor = limited_magnitude / magnitude;
        
        // Apply the scale factor proportionally to both axes
        asymptotic_offset.x = scaledDelta.x * scale_factor;
        asymptotic_offset.y = scaledDelta.y * scale_factor;
    } else {
        asymptotic_offset.x = 0.0F;
        asymptotic_offset.y = 0.0F;
    }
    
    m_window->m_floatingOffset = asymptotic_offset;
    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CMoveTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {

    if (!m_window)
        return;

    if (m_window->m_isFloating || m_lastDelta.size() < 0.1F)
        return;

    // tiled: attempt to move window in the given direction

    const auto WINDOWPOS = m_window->m_realPosition->goal() + m_window->m_floatingOffset;

    m_window->m_floatingOffset = {};

    if (std::abs(m_lastDelta.x) > std::abs(m_lastDelta.y)) {
        // horizontal
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(m_window.lock(), m_lastDelta.x > 0 ? "r" : "l");
    } else {
        // vertical
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(m_window.lock(), m_lastDelta.y > 0 ? "b" : "t");
    }

    const auto GOAL = m_window->m_realPosition->goal();

    m_window->m_realPosition->setValueAndWarp(WINDOWPOS);
    *m_window->m_realPosition = GOAL;

    m_window.reset();
}
