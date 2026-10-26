#pragma once
#include <cmath>

namespace hwmodern {
// The caller supplies observed native presents, including generated output
// when a swapchain counter is available. Queries never count UI draw calls.
// Zero-count polls advance elapsed time; the display updates twice a second.
class PresentedFrameRate {
    double start_ = 0, last_ = 0;
    unsigned long long frames_ = 0;
    float value_ = 0;
    bool started_ = false;
public:
    void reset() noexcept { start_ = last_ = 0; frames_ = 0; value_ = 0; started_ = false; }
    void presented(double seconds, unsigned int count = 1) noexcept {
        if (!std::isfinite(seconds)) return;
        if (!started_ || seconds < last_ || seconds - last_ > 5.0) {
            start_ = last_ = seconds; frames_ = 0; value_ = 0; started_ = true; return;
        }
        last_ = seconds;
        frames_ += count;
        const double elapsed = seconds - start_;
        if (elapsed >= .5) {
            value_ = static_cast<float>(frames_ / elapsed);
            start_ = seconds; frames_ = 0;
        }
    }
    float value() const noexcept { return value_; }
};
}
