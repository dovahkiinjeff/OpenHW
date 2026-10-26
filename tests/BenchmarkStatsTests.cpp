#include "BenchmarkStats.h"
#include "PresentedFrameRate.h"
#undef NDEBUG
#include <cassert>
#include <limits>
int main() {
    hwmodern::BenchmarkMetric metric;
    metric.reset();
    assert(metric.percentile(.99)==0);
    for (int i=1;i<=100;++i) metric.add(i);
    metric.add(-1); metric.add(std::numeric_limits<double>::infinity());
    assert(metric.samples.size()==100 && metric.mean()==50.5);
    assert(metric.percentile(.5)==50 && metric.percentile(.95)==95);
    assert(metric.percentile(.99)==99 && metric.percentile(1)==100);
    assert(metric.over(50)==50);
    metric.reset(); assert(metric.samples.empty() && metric.sum==0);
    for(int i=0;i<70000;++i) metric.add(1);
    assert(metric.samples.size()==65536 && metric.mean()==1);
    hwmodern::PresentedFrameRate fps;
    assert(fps.value() == 0);
    for (int frame=0;frame<=120;++frame) fps.presented(double(frame)/120.0);
    assert(std::abs(fps.value()-120.f)<.01f);
    for (int query=0;query<1000;++query) assert(std::abs(fps.value()-120.f)<.01f);
    fps.presented(std::numeric_limits<double>::quiet_NaN());
    assert(std::abs(fps.value()-120.f)<.01f);
    fps.presented(20); assert(fps.value()==0); // Suspend/focus gap starts a fresh sample.
    fps.presented(19); assert(fps.value()==0); // Non-monotonic reset is safe.
    fps.reset(); assert(fps.value()==0);
    // Native present deltas expose generated output without counting polls.
    fps.presented(0,0);
    for (int frame=1;frame<=60;++frame) fps.presented(double(frame)/60.0,2);
    assert(std::abs(fps.value()-120.f)<.01f); // 60 app frames,120 actual presents.
    fps.presented(1.25,0); assert(std::abs(fps.value()-120.f)<.01f);
    fps.presented(1.5,0); assert(fps.value()==0); // No new native output, not60 FPS.
    fps.reset();fps.presented(0,0);
    for (int poll=1;poll<=60;++poll)
        fps.presented(double(poll)/120.0,poll%4==0?8u:0u);
    assert(std::abs(fps.value()-240.f)<.01f); // Delayed eight-frame batches.
    // Equal timestamps may carry another counter delta; never divide by zero
    // or discard the earlier samples when clock resolution merges two polls.
    fps.reset();fps.presented(0,0);fps.presented(.25,10);
    fps.presented(.25,5);fps.presented(.5,5);assert(fps.value()==40.f);
    // The window accumulator must not overflow a32-bit native counter delta.
    fps.reset();fps.presented(0,0);
    const auto maximumCount=std::numeric_limits<unsigned int>::max();
    fps.presented(.25,maximumCount);fps.presented(.5,maximumCount);
    assert(fps.value()==static_cast<float>(double(maximumCount)*4.0));
    fps.reset();assert(fps.value()==0);
}
