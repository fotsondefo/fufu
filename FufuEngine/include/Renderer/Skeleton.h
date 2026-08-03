#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Fufu
{

struct Bone
{
    std::string name;
    int         parentIndex      = -1;        // -1 = root
    glm::mat4   inverseBindMatrix = glm::mat4(1.f);
};

struct Skeleton
{
    std::vector<Bone>                   bones;
    std::unordered_map<std::string,int> boneIndexMap;
};

struct VecKey  { float time; glm::vec3 value; };
struct QuatKey { float time; glm::quat value; };

struct AnimationChannel
{
    int                  boneIndex = -1;
    std::vector<VecKey>  posKeys;
    std::vector<QuatKey> rotKeys;
    std::vector<VecKey>  scaleKeys;
};

struct AnimationClip
{
    std::string                   name;
    float                         duration    = 0.f;  // in ticks
    float                         ticksPerSec = 25.f;
    std::vector<AnimationChannel> channels;
};

// ── Keyframe sampling ─────────────────────────────────────────────────────

inline glm::vec3 sampleVec(const std::vector<VecKey>& keys, float t,
                             glm::vec3 fallback = glm::vec3(0.f))
{
    if (keys.empty())               return fallback;
    if (t <= keys.front().time)     return keys.front().value;
    if (t >= keys.back().time)      return keys.back().value;
    for (std::size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (t < keys[i + 1].time)
        {
            float f = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return glm::mix(keys[i].value, keys[i + 1].value, f);
        }
    }
    return keys.back().value;
}

inline glm::quat sampleQuat(const std::vector<QuatKey>& keys, float t)
{
    if (keys.empty())               return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (t <= keys.front().time)     return keys.front().value;
    if (t >= keys.back().time)      return keys.back().value;
    for (std::size_t i = 0; i + 1 < keys.size(); ++i)
    {
        if (t < keys[i + 1].time)
        {
            float f = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return glm::normalize(glm::slerp(keys[i].value, keys[i + 1].value, f));
        }
    }
    return keys.back().value;
}

// ── Bone matrix computation ───────────────────────────────────────────────
// Fills outMatrices with finalMatrix[i] = globalPose[i] * inverseBindPose[i].
// tickTime is clip time in ticks. Bones must be in topological order
// (parents before children), which buildSkeleton() guarantees.

inline void computeBoneMatrices(const Skeleton& skeleton,
                                  const AnimationClip& clip,
                                  float tickTime,
                                  std::vector<glm::mat4>& outMatrices)
{
    int numBones = static_cast<int>(skeleton.bones.size());
    outMatrices.assign(numBones, glm::mat4(1.f));
    if (numBones == 0) return;

    std::vector<glm::mat4> local(numBones, glm::mat4(1.f));
    for (const AnimationChannel& ch : clip.channels)
    {
        if (ch.boneIndex < 0 || ch.boneIndex >= numBones) continue;
        glm::vec3 t = sampleVec (ch.posKeys,   tickTime, glm::vec3(0.f));
        glm::quat r = sampleQuat(ch.rotKeys,   tickTime);
        glm::vec3 s = sampleVec (ch.scaleKeys, tickTime, glm::vec3(1.f));
        local[ch.boneIndex] =
            glm::translate(glm::mat4(1.f), t) *
            glm::toMat4(r) *
            glm::scale(glm::mat4(1.f), s);
    }

    std::vector<glm::mat4> global(numBones);
    for (int i = 0; i < numBones; ++i)
    {
        int p = skeleton.bones[i].parentIndex;
        global[i] = (p < 0) ? local[i] : global[p] * local[i];
    }

    for (int i = 0; i < numBones; ++i)
        outMatrices[i] = global[i] * skeleton.bones[i].inverseBindMatrix;
}

} // namespace Fufu
