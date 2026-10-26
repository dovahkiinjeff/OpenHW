#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace hwmodern {
struct BenchmarkMetric {
    std::vector<double> samples;
    double sum = 0.0;
    void reset() { samples.clear(); sum = 0.0; samples.reserve(65536); }
    void add(double value) {
        if (!std::isfinite(value) || value < 0.0 || samples.size() >= 65536) return;
        samples.push_back(value); sum += value;
    }
    double mean() const { return samples.empty() ? 0.0 : sum / samples.size(); }
    double percentile(double fraction) const {
        if (samples.empty()) return 0.0;
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const size_t index = static_cast<size_t>(std::ceil(
            std::clamp(fraction, 0.0, 1.0) * sorted.size()));
        return sorted[index ? index - 1 : 0];
    }
    size_t over(double threshold) const {
        return std::count_if(samples.begin(), samples.end(),
            [threshold](double value) { return value > threshold; });
    }
};
}
