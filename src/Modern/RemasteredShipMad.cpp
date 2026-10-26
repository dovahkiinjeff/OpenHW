#include "RemasteredShipMad.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace {
constexpr size_t MaxBytes = 64 * 1024 * 1024;
constexpr unsigned MaxItems = 100000;
struct Reader {
    const unsigned char *data;
    size_t size, at = 0;
    const unsigned char *take(size_t n) {
        if (n > size - at) throw std::runtime_error("truncated MAD field");
        const auto *p = data + at;
        at += n;
        return p;
    }
    unsigned u32() {
        const auto *p = take(4);
        return unsigned(p[0]) | (unsigned(p[1]) << 8) |
               (unsigned(p[2]) << 16) | (unsigned(p[3]) << 24);
    }
    unsigned be32() {
        const auto *p = take(4);
        return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) |
               (unsigned(p[2]) << 8) | unsigned(p[3]);
    }
    int i32() {
        const unsigned bits = u32();
        int value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    float f32() {
        const unsigned bits = u32();
        float value;
        std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value)) throw std::runtime_error("nonfinite MAD float");
        return value;
    }
    double f64() {
        uint64_t bits = u32();
        bits |= uint64_t(u32()) << 32;
        double value;
        std::memcpy(&value, &bits, 8);
        if (!std::isfinite(value)) throw std::runtime_error("nonfinite MAD key");
        return value;
    }
    unsigned count() {
        const int value = i32();
        if (value < 0 || unsigned(value) > MaxItems)
            throw std::runtime_error("invalid MAD count");
        return unsigned(value);
    }
    void done() const {
        if (at != size) throw std::runtime_error("unconsumed MAD fields");
    }
};
struct Chunk { std::string tag; Reader body; };
std::vector<Chunk> chunks(Reader reader) {
    std::vector<Chunk> result;
    while (reader.at < reader.size) {
        const auto *tagBytes = reader.take(4);
        std::string tag(reinterpret_cast<const char *>(tagBytes), 4);
        const unsigned size = reader.be32();
        Reader body{reader.take(size), size};
        if (tag == "FORM" || tag == "NRML") {
            const auto *inner = body.take(4);
            tag.assign(reinterpret_cast<const char *>(inner), 4);
        }
        result.push_back({tag, {body.data + body.at, body.size - body.at}});
        if (result.size() > MaxItems) throw std::runtime_error("too many MAD chunks");
    }
    return result;
}
Reader find(const std::vector<Chunk> &all, const char *tag) {
    const Chunk *found = nullptr;
    for (const auto &chunk : all) if (chunk.tag == tag) {
        if (found) throw std::runtime_error("duplicate MAD chunk");
        found = &chunk;
    }
    if (!found) throw std::runtime_error("missing MAD chunk");
    return found->body;
}
std::string nameAt(Reader strings, unsigned offset) {
    if (offset >= strings.size || (offset && strings.data[offset - 1] != 0))
        throw std::runtime_error("invalid MAD string offset");
    size_t end = offset;
    while (end < strings.size && strings.data[end]) {
        if (end - offset >= 4096 || strings.data[end] < 32 || strings.data[end] > 126)
            throw std::runtime_error("invalid MAD name");
        ++end;
    }
    if (end == strings.size || end == offset) throw std::runtime_error("unterminated/empty MAD name");
    return std::string(reinterpret_cast<const char *>(strings.data + offset), end - offset);
}
int channel(const std::string &name, const std::string &joint) {
    static const char *suffixes[] = {"translateX", "translateY", "translateZ",
        "rotateX", "rotateY", "rotateZ", "scaleX", "scaleY", "scaleZ"};
    for (int i = 0; i < 9; ++i) if (name == joint + "_" + suffixes[i]) return i;
    throw std::runtime_error("MAD curve does not match track joint/channel");
}
void message(char *error, size_t size, const char *value) {
    if (error && size) std::snprintf(error, size, "%s", value);
}
struct CurveStorage {
    std::string name;
    std::vector<HwRmMadKey> keys;
    int pre = 0, post = 0;
};
struct TrackStorage {
    std::string joint;
    std::vector<unsigned> indices;
};
struct ClipStorage {
    std::string name;
    double start, end, loopStart, loopEnd;
    std::vector<TrackStorage> tracks;
    std::vector<HwRmMadTrack> trackViews;
    bool playable;
};
}
struct HwRmMad {
    unsigned fps = 0;
    std::vector<CurveStorage> curves;
    std::vector<ClipStorage> clips;
    std::vector<HwRmMadCurve> curveViews;
    std::vector<HwRmMadClip> clipViews;
};

extern "C" int hwRmMadRead(const void *data, size_t size, HwRmMad **result,
                            char *error, size_t errorSize) {
    if (!result) { message(error, errorSize, "null MAD output"); return 0; }
    *result = nullptr;
    try {
        if (!data || !size || size > MaxBytes) throw std::runtime_error("invalid MAD input");
        const auto root = chunks({static_cast<const unsigned char *>(data), size});
        if (root.size() != 1 || root[0].tag != "MAD ") throw std::runtime_error("not RM MAD");
        const auto blocks = chunks(root[0].body);
        for (const auto &b : blocks) if (b.tag != "NAME" && b.tag != "VERS" &&
            b.tag != "INFO" && b.tag != "STRI" && b.tag != "MARK" && b.tag != "CURV")
            throw std::runtime_error("unsupported MAD chunk");
        auto version = find(blocks, "VERS");
        if (version.u32() != 0x104) throw std::runtime_error("unsupported MAD version");
        version.done();
        auto info = find(blocks, "INFO");
        auto mad = std::make_unique<HwRmMad>();
        mad->fps = info.count();
        const unsigned clipCount = info.count(), curveCount = info.count();
        const unsigned expectedTracks = info.count(), expectedChannels = info.count();
        info.done();
        if (!mad->fps || mad->fps > 1000 || !clipCount || !curveCount)
            throw std::runtime_error("invalid MAD animation header");
        const auto strings = find(blocks, "STRI");
        auto curves = find(blocks, "CURV");
        mad->curves.reserve(curveCount);
        size_t totalKeys = 0;
        for (unsigned i = 0; i < curveCount; ++i) {
            CurveStorage curve;
            curve.name = nameAt(strings, curves.u32());
            const unsigned count = curves.count();
            if (!count || (totalKeys += count) > 1000000)
                throw std::runtime_error("invalid MAD key count");
            curve.keys.reserve(count);
            for (unsigned k = 0; k < count; ++k) {
                HwRmMadKey key{};
                key.time = curves.f64(); key.value = curves.f64();
                for (float &v : key.inTangent) v = curves.f32();
                for (float &v : key.outTangent) v = curves.f32();
                if (key.time < 0 || (k && key.time < curve.keys.back().time))
                    throw std::runtime_error("unordered/negative MAD key time");
                curve.keys.push_back(key);
            }
            curve.pre = curves.i32(); curve.post = curves.i32();
            mad->curves.push_back(std::move(curve));
        }
        curves.done();
        auto marks = find(blocks, "MARK");
        size_t totalTracks = 0, totalChannels = 0;
        std::unordered_set<std::string> clipNames;
        mad->clips.reserve(clipCount);
        for (unsigned i = 0; i < clipCount; ++i) {
            ClipStorage clip;
            clip.name = nameAt(strings, marks.u32());
            if (!clipNames.insert(clip.name).second) throw std::runtime_error("duplicate MAD clip name");
            clip.start = marks.f32(); clip.end = marks.f32();
            clip.loopStart = marks.f32(); clip.loopEnd = marks.f32();
            const unsigned count = marks.count();
            if ((totalTracks += count) > expectedTracks) throw std::runtime_error("MAD track count mismatch");
            clip.playable = count != 0;
            if (clip.playable && (clip.start < 0 || clip.end < clip.start ||
                clip.loopStart < clip.start || clip.loopEnd > clip.end || clip.loopEnd < clip.loopStart))
                throw std::runtime_error("invalid MAD clip range");
            // Empty source placeholders can use +/-FLT_MAX sentinel ranges.
            // Preserve them for inspection; never fabricate an animation.
            std::unordered_set<std::string> trackNames;
            clip.tracks.reserve(count);
            for (unsigned j = 0; j < count; ++j) {
                TrackStorage track;
                track.joint = nameAt(strings, marks.u32());
                if (!trackNames.insert(track.joint).second) throw std::runtime_error("duplicate MAD clip joint");
                const unsigned channels = marks.count();
                if (!channels || channels > 9 || (totalChannels += channels) > expectedChannels)
                    throw std::runtime_error("invalid MAD channel count");
                unsigned usedChannels = 0;
                for (unsigned c = 0; c < channels; ++c) {
                    const unsigned index = marks.u32();
                    if (index >= curveCount) throw std::runtime_error("invalid MAD curve index");
                    const unsigned bit = 1u << channel(mad->curves[index].name, track.joint);
                    if (usedChannels & bit) throw std::runtime_error("duplicate MAD track channel");
                    usedChannels |= bit;
                    track.indices.push_back(index);
                }
                clip.tracks.push_back(std::move(track));
            }
            mad->clips.push_back(std::move(clip));
        }
        marks.done();
        if (totalTracks != expectedTracks || totalChannels != expectedChannels)
            throw std::runtime_error("MAD aggregate count mismatch");
        // Build borrowed C views only after storage is final, so no vector move
        // can invalidate names or track/curve pointers.
        for (auto &curve : mad->curves) mad->curveViews.push_back({curve.name.c_str(),
            unsigned(curve.keys.size()), curve.keys.data(), curve.pre, curve.post});
        for (auto &clip : mad->clips) {
            for (auto &track : clip.tracks) clip.trackViews.push_back({track.joint.c_str(),
                unsigned(track.indices.size()), track.indices.data()});
            mad->clipViews.push_back({clip.name.c_str(), clip.start, clip.end, clip.loopStart,
                clip.loopEnd, unsigned(clip.tracks.size()), clip.trackViews.data(), clip.playable ? 1 : 0});
        }
        *result = mad.release();
        message(error, errorSize, "");
        return 1;
    } catch (const std::exception &e) { message(error, errorSize, e.what()); return 0; }
}
extern "C" void hwRmMadFree(HwRmMad *mad) { delete mad; }
extern "C" unsigned hwRmMadCurveCount(const HwRmMad *mad) { return mad ? unsigned(mad->curves.size()) : 0; }
extern "C" unsigned hwRmMadClipCount(const HwRmMad *mad) { return mad ? unsigned(mad->clips.size()) : 0; }
extern "C" unsigned hwRmMadFrameRate(const HwRmMad *mad) { return mad ? mad->fps : 0; }
extern "C" const HwRmMadCurve *hwRmMadCurve(const HwRmMad *mad, unsigned index) {
    return mad && index < mad->curveViews.size() ? &mad->curveViews[index] : nullptr;
}
extern "C" const HwRmMadClip *hwRmMadClip(const HwRmMad *mad, unsigned index) {
    return mad && index < mad->clipViews.size() ? &mad->clipViews[index] : nullptr;
}
extern "C" const HwRmMadClip *hwRmMadFindClip(const HwRmMad *mad, const char *name) {
    if (mad && name) for (const auto &clip : mad->clipViews)
        if (std::strcmp(clip.name, name) == 0) return &clip;
    return nullptr;
}
namespace {
double sampleCurve(const CurveStorage &curve, double time) {
    // All audited Kushan sources use constant infinity (0). Reject unfamiliar
    // modes instead of silently changing a future model's animation.
    if (curve.pre != 0 || curve.post != 0)
        throw std::runtime_error("unsupported MAD infinity mode");
    const auto &keys = curve.keys;
    if (time < keys.front().time) return keys.front().value;
    const auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](double t, const HwRmMadKey &key) { return t < key.time; });
    if (next == keys.end()) return keys.back().value;
    const auto &a = *(next - 1), &b = *next;
    if (time == a.time) return a.value;
    // Unweighted XY tangent directions encode dy/dt, not Bezier handles or
    // an extra frame-rate factor. Source keys are already in seconds/radians.
    if (a.outTangent[0] <= 0 || b.inTangent[0] <= 0)
        throw std::runtime_error("unsupported vertical/backward MAD tangent");
    const double dt = b.time - a.time, u = (time - a.time) / dt;
    const double u2 = u*u, u3 = u2*u;
    const double m0 = double(a.outTangent[1]) / a.outTangent[0];
    const double m1 = double(b.inTangent[1]) / b.inTangent[0];
    const double result = (2*u3-3*u2+1)*a.value + (u3-2*u2+u)*dt*m0 +
        (-2*u3+3*u2)*b.value + (u3-u2)*dt*m1;
    if (!std::isfinite(result)) throw std::runtime_error("nonfinite MAD sample");
    return result;
}
}
extern "C" int hwRmMadSampleTrack(const HwRmMad *mad, unsigned clipIndex,
    unsigned trackIndex, double time, const double *defaults, double *output,
    char *error, size_t errorSize) {
    try {
        if (!mad || !defaults || !output || !std::isfinite(time) ||
            clipIndex >= mad->clips.size()) throw std::runtime_error("invalid MAD sample request");
        const auto &clip = mad->clips[clipIndex];
        if (!clip.playable || trackIndex >= clip.tracks.size())
            throw std::runtime_error("unplayable MAD clip/track");
        std::array<double,9> values;
        std::copy(defaults, defaults+9, values.begin());
        for (double value : values) if (!std::isfinite(value))
            throw std::runtime_error("nonfinite MAD defaults");
        time = std::clamp(time, clip.start, clip.end);
        const auto &track = clip.tracks[trackIndex];
        for (unsigned index : track.indices) {
            const auto &curve = mad->curves[index];
            values[channel(curve.name, track.joint)] = sampleCurve(curve, time);
        }
        std::copy(values.begin(), values.end(), output);
        message(error, errorSize, ""); return 1;
    } catch (const std::exception &e) { message(error, errorSize, e.what()); return 0; }
}
extern "C" int hwRmMadValidateJoints(const HwRmMad *mad, const char *const *joints,
                                      unsigned count, char *error, size_t errorSize) {
    try {
        if (!mad || !joints || !count || count > MaxItems) throw std::runtime_error("invalid MAD joint table");
        std::unordered_map<std::string,unsigned> names;
        for (unsigned i = 0; i < count; ++i) {
            if (!joints[i] || !*joints[i]) throw std::runtime_error("invalid HOD joint name");
            ++names[joints[i]];
        }
        for (const auto &clip : mad->clips) for (const auto &track : clip.tracks) {
            if (!names.count(track.joint)) throw std::runtime_error("MAD joint absent from replacement HOD: " + track.joint);
            if (names.at(track.joint) != 1) throw std::runtime_error("ambiguous MAD joint in replacement HOD: " + track.joint);
        }
        message(error, errorSize, ""); return 1;
    } catch (const std::exception &e) { message(error, errorSize, e.what()); return 0; }
}
