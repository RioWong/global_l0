//
// Copyright 2014 Yangbin Lin. All Rights Reserved.
//
// Author: yblin@jmu.edu.cn (Yangbin Lin)
//
// This file is part of the Code Library.
//

#ifndef GEOMETRY_PCA_ESTIMATE_NORMALS_2D_H_
#define GEOMETRY_PCA_ESTIMATE_NORMALS_2D_H_

#include <cassert>

#include "codelibrary/geometry/kernel/center_2d.h"
#include "codelibrary/geometry/kernel/point_2d.h"
#include "codelibrary/util/tree/kd_tree.h"

namespace cl {
namespace geometry {

/**
 * Estimates normal direction by weighted PCA method.
 *
 * The output normal is normalize to the unit length. Its orientation is
 * randomly assigned.
 *
 * Note that, we only need to compute the least eigenvector of the covariance 
 * matrix.
 */
template <typename T>
void PCAEstimateNormal(const Array<Point2D<T>>& points,
                       const Array<T>& weights,
                       Vector2D<T>* normal) {
    static_assert(std::is_floating_point<T>::value);

    assert(!points.empty());
    assert(points.size() == weights.size());
    assert(normal);

    Point2D<T> centroid = Centroid(points, weights);

    T a = 0, b = 0, c = 0;
    int i = 0;
    T sum = 0;
    for (const Point2D<T>& p : points) {
        T x = p.x - centroid.x;
        T y = p.y - centroid.y;
        T w = weights[i++];

        a += w * x * x;
        b += w * x * y;
        c += w * y * y;
        sum += w;
    }

    if (sum == 0) {
        *normal = Vector2D<T>(0, 1);
        return;
    }

    T t = T(1) / sum;
    a *= t;
    b *= t;
    c *= t;

    // [ A  B ]  =  [ cs  -sn ] [ rt1   0  ] [  cs  sn ]
    // [ B  C ]     [ sn   cs ] [  0   rT ] [ -sn  cs ]
    T df = a - c;
    T rt = std::sqrt(df * df + b * b * 4);
    T cs = df > 0 ? df + rt : df - rt;
    T sn = 0;
    if (std::fabs(cs) > std::fabs(b) * 2) {
        t = -b * 2 / cs;
        sn = 1 / std::sqrt(t * t + 1);
        cs = t * sn;
    } else if (std::fabs(b) == 0) {
        cs = 1;
        sn = 0;
    } else {
        t = - cs / b / 2;
        cs = 1 / std::sqrt(t * t + 1);
        sn = t * cs;
    }

    if (df > 0) {
        t = cs;
        cs = -sn;
        sn = t;
    }

    *normal = Vector2D<T>(-sn, cs);
}

/**
 * Estimates normal vector by PCA method.
 *
 * The output normal is normalize to the unit length, and its orientation is
 * randomly assigned.
 */
template <typename T>
void PCAEstimateNormal(const Array<Point2D<T>>& points, Vector2D<T>* normal) {
    static_assert(std::is_floating_point<T>::value);

    Array<T> weights(points.size(), 1);
    PCAEstimateNormal(points, weights, normal);
}

/**
 * Estimates normal vectors by PCA method over the k nearest neighbors.
 *
 * The output normals are normalize to the unit length, and their orientation
 * are randomly assigned.
 *
 * Parameters:
 *   kd_tree  - the input points are stored in the KD tree.
 *   k        - used to define the k-nearest neighbors.
 *   normals  - the output normals.
 */
template <typename T>
void PCAEstimateNormals(const KDTree<Point2D<T>>& kd_tree, int k,
                        Array<Vector2D<T>>* normals) {
    static_assert(std::is_floating_point<T>::value);

    assert(!kd_tree.empty());
    assert(k > 0);
    assert(normals);

    int n = kd_tree.size();
    k = std::min(k, n);

    normals->resize(n);

    const Array<Point2D<T>>& points = kd_tree.points();
    Array<Point2D<T>> neighbors;

    for (int i = 0; i < n; ++i) {
        kd_tree.FindKNearestNeighbors(points[i], k, &neighbors);
        PCAEstimateNormal(neighbors, &(*normals)[i]);
    }
}

/**
 * Estimate a set of normal vectors.
 */
template <typename Iterator, typename T>
void PCAEstimateNormals(Iterator first, Iterator last, int k,
                        Array<Vector2D<T>>* normals) {
    static_assert(std::is_floating_point<T2>::value);

    using Point = typename std::iterator_traits<Iterator>::value_type;
    KDTree<Point> kd_tree(first, last);
    PCAEstimateNormals(kd_tree, k, normals);
}

/**
 * Orientation aware PCA normal estimation.
 *
 * This function will re-orient the normal vector for each point according to
 * the neighbors whose normal vector has the same orientation.
 *
 * Parameters:
 *   kd_tree - the input points are stored in the KD tree.
 *   k       - used to define the k-nearest neighbors.
 *   normals - the output normals.
 */
template <typename T>
void OrientationAwarePCAEstimateNormals(const KDTree<Point2D<T>>& kd_tree, 
                                        int k,
                                        Array<Vector2D<T>>* normals) {
    static_assert(std::is_floating_point<T>::value);

    assert(!kd_tree.empty());
    assert(k > 0);
    assert(normals);
    assert(normals->size() == kd_tree.size());

    int n = kd_tree.size();
    k = std::min(k, n);

    const Array<Point2D<T>>& points = kd_tree.points();
    Array<int> neighbors;

    for (int i = 0; i < n; ++i) {
        kd_tree.FindKNearestNeighbors(points[i], k, &neighbors);

        Array<Point2D<T>> neighbor_points;
        neighbor_points.reserve(k);
        for (int j = 0; j < k; ++j) {
            if ((*normals)[i] * (*normals)[neighbors[j]] >= 0.0) {
                neighbor_points.push_back(points[neighbors[j]]);
            }
        }

        Vector2D<T> normal;
        PCAEstimateNormal(neighbor_points, &normal);
        if (normal * (*normals)[i] < 0) {
            (*normals)[i] = -normal;
        } else {
            (*normals)[i] =  normal;
        }
    }
}

/**
 * Similar to the previous one.
 */
template <typename Iterator, typename T>
void OrientationAwarePCAEstimateNormals(Iterator first, Iterator last, int k,
                                        Array<Vector2D<T>>* normals) {
    static_assert(std::is_floating_point<T>::value);
    using Point = typename std::iterator_traits<Iterator>::value_type;

    KDTree<Point> kd_tree(first, last);
    OrientationAwarePCAEstimateNormals(kd_tree, k, normals);
}

} // namespace geometry
} // namespace cl

#endif // GEOMETRY_PCA_ESTIMATE_NORMALS_2D_H_
