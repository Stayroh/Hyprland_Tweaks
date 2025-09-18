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
    const float SENSITIVITY = 0.02F; // Controls how quickly it approaches the limit
    const float CROSS_AXIS_DAMPING = 0.8F; // Controls how much perpendicular movement is reduced
    
    Vector2D scaledDelta = m_lastDelta * 0.5F;
    Vector2D asymptotic_offset;
    
    // Calculate dominant axis and its strength
    float abs_x = std::abs(scaledDelta.x);
    float abs_y = std::abs(scaledDelta.y);
    float total_magnitude = abs_x + abs_y;
    
    if (total_magnitude > 0.001F) {
        // Calculate directional dominance (0.5 = equal, 1.0 = completely dominant)
        float x_dominance = abs_x / total_magnitude;
        float y_dominance = abs_y / total_magnitude;
        
        // Calculate cross-axis damping factors using exponential function
        // More dominant axis = less damping on perpendicular axis
        float x_cross_damping = std::exp(-CROSS_AXIS_DAMPING * y_dominance * total_magnitude * 0.01F);
        float y_cross_damping = std::exp(-CROSS_AXIS_DAMPING * x_dominance * total_magnitude * 0.01F);
        
        // Apply asymptotic function to X component with cross-axis damping
        if (abs_x > 0.001F) {
            float sign_x = scaledDelta.x > 0 ? 1.0F : -1.0F;
            asymptotic_offset.x = MAX_OFFSET * (1.0F - std::exp(-SENSITIVITY * abs_x)) * sign_x * x_cross_damping;
        } else {
            asymptotic_offset.x = 0.0F;
        }
        
        // Apply asymptotic function to Y component with cross-axis damping
        if (abs_y > 0.001F) {
            float sign_y = scaledDelta.y > 0 ? 1.0F : -1.0F;
            asymptotic_offset.y = MAX_OFFSET * (1.0F - std::exp(-SENSITIVITY * abs_y)) * sign_y * y_cross_damping;
        } else {
            asymptotic_offset.y = 0.0F;
        }
    } else {
        asymptotic_offset.x = 0.0F;
        asymptotic_offset.y = 0.0F;
    }
    
    m_window->m_floatingOffset = asymptotic_offset;
    g_pHyprRenderer->damageWindow(m_window.lock());
}

_pHyprRenderer->damageWindow(m_window.lock());
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
