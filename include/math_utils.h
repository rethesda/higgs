#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#include <algorithm>
#include <optional>
#include <vector>
#include <unordered_set>

#include "RE/havok.h"
#include "RE/offsets.h"

#include "skse64/NiObjects.h"


static constexpr float pi_f = M_PI;
static constexpr float pi_2_f = M_PI_2;
static constexpr float pi_3_2_f = 1.5 * M_PI;

static const float g_minAllowedFingerAngle = 0;// 5 * 0.0174533; // 5 degrees

struct Triangle
{
    UInt16 vertexIndices[3];
};
static_assert(sizeof(Triangle) == 0x06);

struct TriangleData
{
    inline TriangleData(Triangle tri, uintptr_t vertices, UInt32 posOffset, UInt8 vertexSize) {
        uintptr_t vert = (vertices + tri.vertexIndices[0] * vertexSize);
        v0 = *(NiPoint3 *)(vert + posOffset);
        vert = (vertices + tri.vertexIndices[1] * vertexSize);
        v1 = *(NiPoint3 *)(vert + posOffset);
        vert = (vertices + tri.vertexIndices[2] * vertexSize);
        v2 = *(NiPoint3 *)(vert + posOffset);
    }

    inline TriangleData(Triangle tri, const std::vector<NiPoint3> &vertices) {
        v0 = vertices[tri.vertexIndices[0]];
        v1 = vertices[tri.vertexIndices[1]];
        v2 = vertices[tri.vertexIndices[2]];
    }

    TriangleData() : v0(), v1(), v2() {}

    inline void ApplyTransform(NiTransform &transform) {
        v0 = transform * v0;
        v1 = transform * v1;
        v2 = transform * v2;
    }

    NiPoint3 v0;
    NiPoint3 v1;
    NiPoint3 v2;
};

struct PartitionData
{
    PartitionData() {};
    PartitionData(NiSkinInstance *skinInstance, uintptr_t vertexData) : skinInstance(skinInstance), vertexData(vertexData) {}

    std::vector<NiPoint3> verticesWS{};
    std::vector<UInt16> globalVertToPartVertMap{};
    uintptr_t vertexData = 0;
    NiSkinInstance *skinInstance = nullptr;
};

struct TrianglePartitionData
{
    NiSkinPartition::Partition &partition;
    Triangle indices;
};

struct Intersection
{
    float angle; // angle of the fingertip at intersection pt
    SInt32 triangleIndex; // the index within the array of triangles that was intersected
};

struct OldIntersection
{
    BSTriShape *node; // the trishape where the intersected triangle resides
    Triangle tri; // triangle that was intersected
    float angle; // angle of the fingertip at intersection pt
};

struct Point2
{
    float x;
    float y;

    Point2();
    Point2(float X, float Y) : x(X), y(Y) { };

    Point2 Point2::operator- () const;
    Point2 Point2::operator+ (const Point2& pt) const;

    Point2 Point2::operator- (const Point2& pt) const;

    Point2& Point2::operator+= (const Point2& pt);
    Point2& Point2::operator-= (const Point2& pt);

    // Scalar operations
    Point2 Point2::operator* (float scalar) const;
    Point2 Point2::operator/ (float scalar) const;

    Point2& Point2::operator*= (float scalar);
    Point2& Point2::operator/= (float scalar);
};

namespace MathUtils
{
    struct Result
    {
        float sqrDistance;
        // barycentric coordinates for triangle.v[3]
        float parameter[3];
        NiPoint3 closest;
    };

    Result GetClosestPointOnTriangle(const NiPoint3 &point, const TriangleData &triangle);
}

struct SkinInstanceRepresentation
{
    NiSkinData *skinData;
    NiSkinPartition *skinPartition;
    NiTransform **boneTransforms;
    UInt32 numBones;
};


inline float VectorLengthSquared(const NiPoint3 &vec) { return vec.x*vec.x + vec.y*vec.y + vec.z*vec.z; }
inline float VectorLengthSquared(const Point2 &vec) { return vec.x*vec.x + vec.y*vec.y; }
inline float VectorLength(const NiPoint3 &vec) { return sqrtf(VectorLengthSquared(vec)); }
inline float VectorLength(const Point2 &vec) { return sqrtf(VectorLengthSquared(vec)); }
inline NiPoint3 VectorAbs(const NiPoint3 &vec) { return { fabs(vec.x), fabs(vec.y), fabs(vec.z) }; }
inline float DotProduct(const NiPoint3 &vec1, const NiPoint3 &vec2) { return vec1.x*vec2.x + vec1.y*vec2.y + vec1.z*vec2.z; }
inline float DotProductSafe(const NiPoint3 &vec1, const NiPoint3 &vec2) { return std::clamp(DotProduct(vec1, vec2), -1.f, 1.f); }
inline float DotProduct(const Point2 &vec1, const Point2 &vec2) { return vec1.x*vec2.x + vec1.y*vec2.y; }
inline float DotProduct(const NiQuaternion &q1, const NiQuaternion &q2) { return q1.m_fW*q2.m_fW + q1.m_fX*q2.m_fX + q1.m_fY*q2.m_fY + q1.m_fZ*q2.m_fZ; }
inline float DotProductSafe(const NiQuaternion &q1, const NiQuaternion &q2) { return std::clamp(DotProduct(q1, q2), -1.f, 1.f); }
inline float QuaternionLength(const NiQuaternion &q) { return sqrtf(DotProduct(q, q)); }
inline NiPoint3 VectorNormalized(const NiPoint3 &vec) { float length = VectorLength(vec); return length > 0.0f ? vec / length : NiPoint3(); }
NiPoint3 CrossProduct(const NiPoint3 &vec1, const NiPoint3 &vec2);
NiMatrix33 MatrixFromAxisAngle(const NiPoint3 &axis, float theta);
float RotationAngle(const NiMatrix33 &rot);
std::pair<NiPoint3, float> QuaternionToAxisAngle(const NiQuaternion &q);
NiPoint3 NiMatrixToYawPitchRoll(NiMatrix33 &mat);
NiPoint3 NiMatrixToEuler(NiMatrix33 &mat);
NiPoint3 MatrixToEuler(const NiMatrix33 &mat);
NiMatrix33 EulerToMatrix(const NiPoint3 &euler);
NiPoint3 NifskopeMatrixToEuler(const NiMatrix33 &in);
NiMatrix33 NifskopeEulerToMatrix(const NiPoint3 &in);
NiMatrix33 MatrixFromForwardVector(NiPoint3 &forward, NiPoint3 &world);
NiPoint3 RotateVectorByAxisAngle(const NiPoint3 &vector, const NiPoint3 &axis, float angle);
NiPoint3 RotateVectorByQuaternion(const NiQuaternion &quat, const NiPoint3 &vec);
NiPoint3 RotateVectorByInverseQuaternion(const NiQuaternion &quat, const NiPoint3 &vec);
NiPoint3 ProjectVectorOntoPlane(const NiPoint3 &vector, const NiPoint3 &normal);
NiTransform RotateTransformAboutPoint(NiTransform &transform, NiPoint3 &point, NiMatrix33 &rotation);
std::pair<NiQuaternion, NiQuaternion> SwingTwistDecomposition(NiQuaternion &rotation, NiPoint3 &direction);
void NiMatrixToHkMatrix(const NiMatrix33 &niMat, hkMatrix3 &hkMat);
hkRotation NiMatrixToHkMatrix(const NiMatrix33 &niMat);
void HkMatrixToNiMatrix(const hkMatrix3 &hkMat, NiMatrix33 &niMat);
NiMatrix33 HkMatrixToNiMatrix(const hkMatrix3 &hkMat);
NiTransform hkTransformToNiTransform(const hkTransform &t, float scale = 1.f, bool useHavokScale = true);
hkTransform NiTransformTohkTransform(const NiTransform &t, bool useHavokScale = true);
NiMatrix33 QuaternionToMatrix(const NiQuaternion &q);
inline NiQuaternion MatrixToQuaternion(const NiMatrix33 &m) { NiQuaternion q; NiMatrixToNiQuaternion(q, m); return q; }
inline NiQuaternion HkQuatToNiQuat(const hkQuaternion &quat) { return { quat.m_vec(3), quat.m_vec(0), quat.m_vec(1), quat.m_vec(2) }; }
inline hkQuaternion NiQuatToHkQuat(const NiQuaternion &quat) { return hkQuaternion(quat.m_fX, quat.m_fY, quat.m_fZ, quat.m_fW); }
inline NiPoint3 HkVectorToNiPoint(const hkVector4 &vec) { return { vec.getQuad().m128_f32[0], vec.getQuad().m128_f32[1], vec.getQuad().m128_f32[2] }; }
inline hkVector4 NiPointToHkVector(const NiPoint3 &pt) { return { pt.x, pt.y, pt.z, 0 }; };
inline hkVector4 NiPointToHkVector(const NiPoint3 &pt, float w) { return { pt.x, pt.y, pt.z, w }; };
inline NiTransform InverseTransform(const NiTransform &t) { NiTransform inverse; t.Invert(inverse); return inverse; }
inline NiPoint3 RightVector(const NiMatrix33 &r) { return { r.data[0][0], r.data[1][0], r.data[2][0] }; }
inline NiPoint3 ForwardVector(const NiMatrix33 &r) { return { r.data[0][1], r.data[1][1], r.data[2][1] }; }
inline NiPoint3 UpVector(const NiMatrix33 &r) { return { r.data[0][2], r.data[1][2], r.data[2][2] }; }
inline void SetRightVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][0] = v.x; r.data[1][0] = v.y; r.data[2][0] = v.z; }
inline void SetForwardVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][1] = v.x; r.data[1][1] = v.y; r.data[2][1] = v.z; }
inline void SetUpVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][2] = v.x; r.data[1][2] = v.y; r.data[2][2] = v.z; }
NiQuaternion QuaternionIdentity();
NiQuaternion QuaternionNormalized(const NiQuaternion &q);
NiQuaternion QuaternionMultiply(const NiQuaternion &qa, const NiQuaternion &qb);
NiQuaternion QuaternionMultiply(const NiQuaternion &q, float multiplier);
NiQuaternion QuaternionInverse(const NiQuaternion &q);
inline float QuaternionAngle(const NiQuaternion &qa, const NiQuaternion &qb) { return 2.0f * acosf(abs(DotProductSafe(qa, qb))); }
NiQuaternion slerp(const NiQuaternion &qa, const NiQuaternion &qb, double t);
inline NiPoint3 lerp(const NiPoint3 &a, const NiPoint3 &b, float t) { return a * (1.0f - t) + b * t; }
inline float lerp(float a, float b, float t) { return a * (1.0f - t) + b * t; }
NiTransform lerp(NiTransform& a, NiTransform& b, double t);
inline float logistic(float x, float k, float midpoint) { return 1.0f / (1.0f + expf(-k * (x - midpoint))); };
float AdvanceFloat(float current, float target, float speed);
NiPoint3 AdvancePosition(const NiPoint3 &currentPos, const NiPoint3 &targetPos, float speed);
std::optional<NiTransform> AdvanceTransform(const NiTransform &currentTransform, const NiTransform &targetTransform, float posSpeed, float rotSpeed);
std::optional<NiTransform> AdvanceTransformSpeedMultiplied(const NiTransform &currentTransform, const NiTransform &targetTransform, float posSpeedMult, float rotSpeedMult);
float Determinant33(const NiMatrix33 &m);
NiPoint3 QuadraticFromPoints(const NiPoint2 &p1, const NiPoint2 &p2, const NiPoint2 &p3);
inline float ConstrainAngle180(float x) { x = fmodf(x + M_PI, 2*M_PI); if (x < 0) x += 2*M_PI; return x - M_PI; }
inline float ConstrainAngle360(float x) { x = fmod(x, 2*M_PI); if (x < 0) x += 2*M_PI; return x; }
inline float ConstrainAngleNegative360(float x) { return -ConstrainAngle360(-x); }

bool ShouldIgnoreBasedOnVertexAlpha(BSTriShape *geom);

void GetSkinnedTriangles(NiAVObject *root, std::vector<TriangleData> &triangles, std::vector<TrianglePartitionData> &trianglePartitions, std::unordered_map<NiSkinPartition::Partition *, PartitionData> &partitionData, std::vector<SkinInstanceRepresentation> &visitedSkinInstances, std::unordered_set<NiAVObject *> *nodesToSkinTo = nullptr);
void GetTriangles(NiAVObject *root, std::vector<TriangleData> &triangles, std::vector<NiAVObject *> &triangleNodes);

bool GetIntersections(const std::vector<TriangleData> &triangles, int fingerIndex, float handScale, const NiPoint3 &center, const NiPoint3 &normal, const NiPoint3 &zeroAngleVector,
    Intersection &outIntersection);
void GetFingerIntersectionOnGraphicsGeometry(std::vector<Intersection> &tipIntersections, std::vector<Intersection> &outerIntersections, std::vector<Intersection> &innerIntersections,
    const std::vector<TriangleData> &triangles,
    int fingerIndex, float handScale, const NiPoint3 &center, const NiPoint3 &normal, const NiPoint3 &zeroAngleVector);
bool GetClosestPointOnGraphicsGeometry(NiAVObject *root, const NiPoint3 &point, NiPoint3 *closestPos, NiPoint3 *closestNormal, float *closestDistanceSoFar);
bool GetClosestPointOnGraphicsGeometryToLine(const std::vector<TriangleData> &triangles, const NiPoint3 &point, const NiPoint3 &direction,
    NiPoint3 &closestPos, NiPoint3 &closestNormal, int &closestIndex, float &closestDistanceSoFar);



namespace NiMathDouble
{
    class NiPoint3
    {
    public:
        double	x;	// 0
        double	y;	// 4
        double	z;	// 8

        NiPoint3();
        NiPoint3(double X, double Y, double Z) : x(X), y(Y), z(Z) { };
        NiPoint3(const ::NiPoint3 &pointSingle);

        ::NiPoint3 ToSingle() const { return ::NiPoint3(x, y, z); }

        // Negative
        NiPoint3 operator- () const;

        // Basic operations
        NiPoint3 operator+ (const NiPoint3 &pt) const;
        NiPoint3 operator- (const NiPoint3 &pt) const;

        NiPoint3 &operator+= (const NiPoint3 &pt);
        NiPoint3 &operator-= (const NiPoint3 &pt);

        // Scalar operations
        NiPoint3 operator* (double fScalar) const;
        NiPoint3 operator/ (double fScalar) const;

        NiPoint3 &operator*= (double fScalar);
        NiPoint3 &operator/= (double fScalar);
    };

    class NiMatrix33
    {
    public:
        union
        {
            double	data[3][3];
            double   arr[9];
        };

        NiMatrix33() = default;
        NiMatrix33(const ::NiMatrix33 &matSingle);

        ::NiMatrix33 ToSingle() const;

        void Identity();

        // Matric mult
        NiMatrix33 operator*(const NiMatrix33 &mat) const;

        // Vector mult
        NiPoint3 operator*(const NiPoint3 &pt) const;

        // Scalar multiplier
        NiMatrix33 operator*(double fScalar) const;

        NiMatrix33 Transpose() const;
    };

    // 34
    class NiTransform
    {
    public:
        NiMatrix33	rot;	// 00
        NiPoint3	pos;	// 24
        double		scale;	// 30

        NiTransform();
        NiTransform(const ::NiTransform &transformSingle);

        ::NiTransform ToSingle() const;

        // Multiply transforms
        NiTransform operator*(const NiTransform &rhs) const;

        // Transform point
        NiPoint3 operator*(const NiPoint3 &pt) const;

        // Invert
        void Invert(NiTransform &kDest) const;
    };

    inline NiTransform InverseTransform(const NiTransform &t) { NiTransform inverse; t.Invert(inverse); return inverse; }
    inline double VectorLengthSquared(const NiPoint3 &vec) { return vec.x*vec.x + vec.y*vec.y + vec.z*vec.z; }
    inline double VectorLength(const NiPoint3 &vec) { return sqrt(VectorLengthSquared(vec)); }
    inline NiPoint3 VectorNormalized(const NiPoint3 &vec) { double length = VectorLength(vec); return length > 0.0 ? vec / length : NiPoint3(); }
    inline NiPoint3 RightVector(const NiMatrix33 &r) { return { r.data[0][0], r.data[1][0], r.data[2][0] }; }
    inline NiPoint3 ForwardVector(const NiMatrix33 &r) { return { r.data[0][1], r.data[1][1], r.data[2][1] }; }
    inline NiPoint3 UpVector(const NiMatrix33 &r) { return { r.data[0][2], r.data[1][2], r.data[2][2] }; }
    inline void SetRightVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][0] = v.x; r.data[1][0] = v.y; r.data[2][0] = v.z; }
    inline void SetForwardVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][1] = v.x; r.data[1][1] = v.y; r.data[2][1] = v.z; }
    inline void SetUpVector(NiMatrix33 &r, const NiPoint3 &v) { r.data[0][2] = v.x; r.data[1][2] = v.y; r.data[2][2] = v.z; }
    inline double DotProduct(const NiPoint3 &vec1, const NiPoint3 &vec2) { return vec1.x*vec2.x + vec1.y*vec2.y + vec1.z*vec2.z; }

    void UpdateNode(NiAVObject *node);
    void UpdateTransform(NiAVObject *node, const NiTransform &transform);
    NiTransform UpdateClavicleToTransformHand(NiAVObject *a_clavicle, NiAVObject *a_hand, NiTransform *a_wandNodeTransformWorld, NiTransform *a_magicHandTransformLocal);
}

namespace NiMathSingle
{
    void UpdateNode(NiAVObject *node);
}
