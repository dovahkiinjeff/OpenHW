use crate::types::{HODQuaternion, Matrix4, Vector3};

pub fn compose_transform_matrix(pos: Vector3, rot: Vector3, scale: Vector3) -> Matrix4 {
    let cx = rot.x.cos();
    let sx = rot.x.sin();
    let cy = rot.y.cos();
    let sy = rot.y.sin();
    let cz = rot.z.cos();
    let sz = rot.z.sin();

    // Enforce a minimum scale of 1.0 if the parsed scale is zero or near-zero
    // to prevent matrix multiplication from collapsing child translations to (0,0,0)
    let s_x = if scale.x.abs() < 0.0001 { 1.0 } else { scale.x };
    let s_y = if scale.y.abs() < 0.0001 { 1.0 } else { scale.y };
    let s_z = if scale.z.abs() < 0.0001 { 1.0 } else { scale.z };

    let mut ms = [[0.0f32; 4]; 4];
    ms[0][0] = s_x;
    ms[1][1] = s_y;
    ms[2][2] = s_z;
    ms[3][3] = 1.0;

    let mut rx = [[0.0f32; 4]; 4];
    rx[0][0] = 1.0;
    rx[1][1] = cx;
    rx[1][2] = sx;
    rx[2][1] = -sx;
    rx[2][2] = cx;
    rx[3][3] = 1.0;

    let mut ry = [[0.0f32; 4]; 4];
    ry[0][0] = cy;
    ry[0][2] = -sy;
    ry[1][1] = 1.0;
    ry[2][0] = sy;
    ry[2][2] = cy;
    ry[3][3] = 1.0;

    let mut rz = [[0.0f32; 4]; 4];
    rz[0][0] = cz;
    rz[0][1] = sz;
    rz[1][0] = -sz;
    rz[1][1] = cz;
    rz[2][2] = 1.0;
    rz[3][3] = 1.0;

    fn mat_mul(a: [[f32; 4]; 4], b: [[f32; 4]; 4]) -> [[f32; 4]; 4] {
        let mut res = [[0.0f32; 4]; 4];
        for i in 0..4 {
            for j in 0..4 {
                res[i][j] =
                    a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
            }
        }
        res
    }

    let m_s_rx = mat_mul(ms, rx);
    let m_s_rx_ry = mat_mul(m_s_rx, ry);
    let mut final_rot = mat_mul(m_s_rx_ry, rz);

    final_rot[3][0] = pos.x;
    final_rot[3][1] = pos.y;
    final_rot[3][2] = pos.z;
    final_rot[3][3] = 1.0;

    Matrix4 { m: final_rot }
}

pub fn decompose_matrix(matrix: Matrix4) -> (Vector3, Vector3, Vector3) {
    let m = matrix.m;
    let pos = Vector3 {
        x: m[3][0],
        y: m[3][1],
        z: m[3][2],
    };

    let sx = (m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]).sqrt();
    let sy = (m[1][0] * m[1][0] + m[1][1] * m[1][1] + m[1][2] * m[1][2]).sqrt();
    let sz = (m[2][0] * m[2][0] + m[2][1] * m[2][1] + m[2][2] * m[2][2]).sqrt();
    let scale = Vector3 {
        x: sx,
        y: sy,
        z: sz,
    };

    let r13 = if sx > 0.0 { m[0][2] / sx } else { 0.0 };
    let r12 = if sx > 0.0 { m[0][1] / sx } else { 0.0 };
    let r11 = if sx > 0.0 { m[0][0] / sx } else { 0.0 };
    let r23 = if sy > 0.0 { m[1][2] / sy } else { 0.0 };
    let r33 = if sz > 0.0 { m[2][2] / sz } else { 0.0 };
    let r21 = if sy > 0.0 { m[1][0] / sy } else { 0.0 };
    let r22 = if sy > 0.0 { m[1][1] / sy } else { 0.0 };

    let mut rx = 0.0;
    let mut ry = 0.0;
    let mut rz = 0.0;

    if r13.abs() < 0.99999 {
        ry = (-r13).asin();
        rx = r23.atan2(r33);
        rz = r12.atan2(r11);
    } else {
        ry = if r13 < 0.0 {
            std::f32::consts::FRAC_PI_2
        } else {
            -std::f32::consts::FRAC_PI_2
        };
        rx = -r21.atan2(r22);
        rz = 0.0;
    }

    let rot = Vector3 {
        x: rx,
        y: ry,
        z: rz,
    };
    (pos, rot, scale)
}

pub(crate) struct ParsedCurve {
    pub(crate) name: String,
    pub(crate) keyframes: Vec<HODKeyframeCurveValue>,
    #[allow(dead_code)]
    pub(crate) pre_infinity: i32,
    #[allow(dead_code)]
    pub(crate) post_infinity: i32,
}

pub(crate) struct HODKeyframeCurveValue {
    pub(crate) time: f64,
    pub(crate) value: f64,
    pub(crate) in_tangent: [f32; 2],
    pub(crate) out_tangent: [f32; 2],
}

pub(crate) fn evaluate_curve(keyframes: &[HODKeyframeCurveValue], time: f64, default_val: f64) -> f64 {
    if keyframes.is_empty() {
        return default_val;
    }
    if keyframes.len() == 1 {
        return keyframes[0].value;
    }
    if time <= keyframes[0].time {
        return keyframes[0].value;
    }
    if time >= keyframes[keyframes.len() - 1].time {
        return keyframes[keyframes.len() - 1].value;
    }
    for i in 0..keyframes.len() - 1 {
        let k1 = &keyframes[i];
        let k2 = &keyframes[i + 1];
        if time >= k1.time && time <= k2.time {
            let dt = k2.time - k1.time;
            if dt.abs() < 1e-6 {
                return k1.value;
            }
            let alpha = (time - k1.time) / dt;
            return k1.value + alpha * (k2.value - k1.value);
        }
    }
    keyframes[0].value
}

pub(crate) fn quaternion_to_euler(q: &HODQuaternion) -> Vector3 {
    let x = q.x;
    let y = q.y;
    let z = q.z;
    let w = q.w;

    let mut m = [[0.0f32; 4]; 4];
    m[0][0] = 1.0 - 2.0 * (y * y + z * z);
    m[0][1] = 2.0 * (x * y + z * w);
    m[0][2] = 2.0 * (x * z - y * w);

    m[1][0] = 2.0 * (x * y - z * w);
    m[1][1] = 1.0 - 2.0 * (x * x + z * z);
    m[1][2] = 2.0 * (y * z + x * w);

    m[2][0] = 2.0 * (x * z + y * w);
    m[2][1] = 2.0 * (y * z - x * w);
    m[2][2] = 1.0 - 2.0 * (x * x + y * y);
    m[3][3] = 1.0;

    let (_, rot, _) = decompose_matrix(Matrix4 { m });
    rot
}

pub fn euler_to_quaternion(rot: &Vector3) -> HODQuaternion {
    let cx = rot.x.cos();
    let sx = rot.x.sin();
    let cy = rot.y.cos();
    let sy = rot.y.sin();
    let cz = rot.z.cos();
    let sz = rot.z.sin();

    let mut rx = [[0.0f32; 3]; 3];
    rx[0][0] = 1.0;
    rx[1][1] = cx;
    rx[1][2] = sx;
    rx[2][1] = -sx;
    rx[2][2] = cx;

    let mut ry = [[0.0f32; 3]; 3];
    ry[0][0] = cy;
    ry[0][2] = -sy;
    ry[1][1] = 1.0;
    ry[2][0] = sy;
    ry[2][2] = cy;

    let mut rz = [[0.0f32; 3]; 3];
    rz[0][0] = cz;
    rz[0][1] = sz;
    rz[1][0] = -sz;
    rz[1][1] = cz;
    rz[2][2] = 1.0;

    fn mat_mul_3(a: [[f32; 3]; 3], b: [[f32; 3]; 3]) -> [[f32; 3]; 3] {
        let mut res = [[0.0f32; 3]; 3];
        for i in 0..3 {
            for j in 0..3 {
                res[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
            }
        }
        res
    }

    let rot_m = mat_mul_3(mat_mul_3(rx, ry), rz);
    matrix_to_quaternion(rot_m)
}

pub(crate) fn matrix_to_quaternion(m: [[f32; 3]; 3]) -> HODQuaternion {
    let tr = m[0][0] + m[1][1] + m[2][2];

    let (qw, qx, qy, qz);
    if tr > 0.0 {
        let s = (tr + 1.0).sqrt() * 2.0;
        qw = 0.25 * s;
        qx = (m[1][2] - m[2][1]) / s;
        qy = (m[2][0] - m[0][2]) / s;
        qz = (m[0][1] - m[1][0]) / s;
    } else if (m[0][0] > m[1][1]) && (m[0][0] > m[2][2]) {
        let s = (1.0 + m[0][0] - m[1][1] - m[2][2]).sqrt() * 2.0;
        qw = (m[1][2] - m[2][1]) / s;
        qx = 0.25 * s;
        qy = (m[0][1] + m[1][0]) / s;
        qz = (m[2][0] + m[0][2]) / s;
    } else if m[1][1] > m[2][2] {
        let s = (1.0 + m[1][1] - m[0][0] - m[2][2]).sqrt() * 2.0;
        qw = (m[2][0] - m[0][2]) / s;
        qx = (m[0][1] + m[1][0]) / s;
        qy = 0.25 * s;
        qz = (m[1][2] + m[2][1]) / s;
    } else {
        let s = (1.0 + m[2][2] - m[0][0] - m[1][1]).sqrt() * 2.0;
        qw = (m[0][1] - m[1][0]) / s;
        qx = (m[2][0] + m[0][2]) / s;
        qy = (m[1][2] + m[2][1]) / s;
        qz = 0.25 * s;
    }

    HODQuaternion {
        x: qx,
        y: qy,
        z: qz,
        w: qw,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_close(actual: f32, expected: f32, tolerance: f32) {
        assert!((actual - expected).abs() <= tolerance, "{actual} != {expected}");
    }

    #[test]
    fn math_compose_decompose_roundtrip() {
        let pos = Vector3 { x: 1.5, y: -2.25, z: 3.0 };
        let rot = Vector3 { x: 0.3, y: -0.7, z: 1.1 };
        let scale = Vector3 { x: 2.0, y: 0.5, z: 1.25 };
        let matrix = compose_transform_matrix(pos.clone(), rot.clone(), scale.clone());
        let (actual_pos, actual_rot, actual_scale) = decompose_matrix(matrix);

        for (actual, expected) in [
            (actual_pos.x, pos.x),
            (actual_pos.y, pos.y),
            (actual_pos.z, pos.z),
            (actual_rot.x, rot.x),
            (actual_rot.y, rot.y),
            (actual_rot.z, rot.z),
            (actual_scale.x, scale.x),
            (actual_scale.y, scale.y),
            (actual_scale.z, scale.z),
        ] {
            assert_close(actual, expected, 1e-4);
        }
    }

    #[test]
    fn math_quaternion_euler_roundtrip() {
        let rot = Vector3 { x: 0.3, y: -0.7, z: 1.1 };
        let quaternion = euler_to_quaternion(&rot);
        let actual = quaternion_to_euler(&quaternion);

        assert_close(actual.x, rot.x, 1e-3);
        assert_close(actual.y, rot.y, 1e-3);
        assert_close(actual.z, rot.z, 1e-3);
    }
}
