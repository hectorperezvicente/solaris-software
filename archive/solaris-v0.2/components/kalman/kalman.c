#include <math.h>
#include <string.h>
#include "kalman.h"

#define KALMAN_EPS 1.0e-9f

static void mat4_zero(float A[16])
{
    memset(A, 0, 16U * sizeof(float));
}

static void mat3_zero(float A[9])
{
    memset(A, 0, 9U * sizeof(float));
}

static void mat4_identity(float A[16])
{
    mat4_zero(A);
    A[0] = 1.0f;
    A[5] = 1.0f;
    A[10] = 1.0f;
    A[15] = 1.0f;
}

static void mat4_transpose(const float A[16], float AT[16])
{
    AT[0] = A[0];
    AT[1] = A[4];
    AT[2] = A[8];
    AT[3] = A[12];

    AT[4] = A[1];
    AT[5] = A[5];
    AT[6] = A[9];
    AT[7] = A[13];

    AT[8] = A[2];
    AT[9] = A[6];
    AT[10] = A[10];
    AT[11] = A[14];

    AT[12] = A[3];
    AT[13] = A[7];
    AT[14] = A[11];
    AT[15] = A[15];
}

static void mat3_transpose(const float A[9], float AT[9])
{
    AT[0] = A[0];
    AT[1] = A[3];
    AT[2] = A[6];

    AT[3] = A[1];
    AT[4] = A[4];
    AT[5] = A[7];

    AT[6] = A[2];
    AT[7] = A[5];
    AT[8] = A[8];
}

static void mat3x4_transpose_to_4x3(const float A[12], float AT[12])
{
    /*
     * A: 3x4 row-major
     * AT: 4x3 row-major
     */
    AT[0] = A[0];
    AT[1] = A[4];
    AT[2] = A[8];

    AT[3] = A[1];
    AT[4] = A[5];
    AT[5] = A[9];

    AT[6] = A[2];
    AT[7] = A[6];
    AT[8] = A[10];

    AT[9] = A[3];
    AT[10] = A[7];
    AT[11] = A[11];
}

static void mat4x3_transpose_to_3x4(const float A[12], float AT[12])
{
    /*
     * A: 4x3 row-major
     * AT: 3x4 row-major
     */
    AT[0] = A[0];
    AT[1] = A[3];
    AT[2] = A[6];
    AT[3] = A[9];

    AT[4] = A[1];
    AT[5] = A[4];
    AT[6] = A[7];
    AT[7] = A[10];

    AT[8] = A[2];
    AT[9] = A[5];
    AT[10] = A[8];
    AT[11] = A[11];
}

static void mat4_add(const float A[16], const float B[16], float C[16])
{
    for (int i = 0; i < 16; i++)
    {
        C[i] = A[i] + B[i];
    }
}

static void mat4_sub(const float A[16], const float B[16], float C[16])
{
    for (int i = 0; i < 16; i++)
    {
        C[i] = A[i] - B[i];
    }
}

static void mat4_mul(const float A[16], const float B[16], float C[16])
{
    float T[16];

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            T[4 * r + c] = A[4 * r + 0] * B[0 * 4 + c] + A[4 * r + 1] * B[1 * 4 + c] +
                           A[4 * r + 2] * B[2 * 4 + c] + A[4 * r + 3] * B[3 * 4 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat4x4_mul_4x3(const float A[16], const float B[12], float C[12])
{
    /*
     * A: 4x4
     * B: 4x3
     * C: 4x3
     */
    float T[12];

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            T[3 * r + c] = A[4 * r + 0] * B[0 * 3 + c] + A[4 * r + 1] * B[1 * 3 + c] +
                           A[4 * r + 2] * B[2 * 3 + c] + A[4 * r + 3] * B[3 * 3 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat3x4_mul_4x4(const float A[12], const float B[16], float C[12])
{
    /*
     * A: 3x4
     * B: 4x4
     * C: 3x4
     */
    float T[12];

    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            T[4 * r + c] = A[4 * r + 0] * B[0 * 4 + c] + A[4 * r + 1] * B[1 * 4 + c] +
                           A[4 * r + 2] * B[2 * 4 + c] + A[4 * r + 3] * B[3 * 4 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat3x4_mul_4x3(const float A[12], const float B[12], float C[9])
{
    /*
     * A: 3x4
     * B: 4x3
     * C: 3x3
     */
    float T[9];

    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            T[3 * r + c] = A[4 * r + 0] * B[0 * 3 + c] + A[4 * r + 1] * B[1 * 3 + c] +
                           A[4 * r + 2] * B[2 * 3 + c] + A[4 * r + 3] * B[3 * 3 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat4x3_mul_3x3(const float A[12], const float B[9], float C[12])
{
    /*
     * A: 4x3
     * B: 3x3
     * C: 4x3
     */
    float T[12];

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            T[3 * r + c] = A[3 * r + 0] * B[0 * 3 + c] + A[3 * r + 1] * B[1 * 3 + c] +
                           A[3 * r + 2] * B[2 * 3 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat4x3_mul_3x4(const float A[12], const float B[12], float C[16])
{
    /*
     * A: 4x3
     * B: 3x4
     * C: 4x4
     */
    float T[16];

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            T[4 * r + c] = A[3 * r + 0] * B[0 * 4 + c] + A[3 * r + 1] * B[1 * 4 + c] +
                           A[3 * r + 2] * B[2 * 4 + c];
        }
    }

    memcpy(C, T, sizeof(T));
}

static void mat4x3_mul_vec3(const float A[12], const float x[3], float y[4])
{
    for (int r = 0; r < 4; r++)
    {
        y[r] = A[3 * r + 0] * x[0] + A[3 * r + 1] * x[1] + A[3 * r + 2] * x[2];
    }
}

static int mat3_inverse(const float A[9], float invA[9])
{
    const float c00 = A[4] * A[8] - A[5] * A[7];
    const float c01 = A[5] * A[6] - A[3] * A[8];
    const float c02 = A[3] * A[7] - A[4] * A[6];

    const float det = A[0] * c00 + A[1] * c01 + A[2] * c02;

    if (fabsf(det) < 1.0e-9f)
    {
        return 0;
    }

    const float inv_det = 1.0f / det;

    invA[0] = c00 * inv_det;
    invA[1] = (A[2] * A[7] - A[1] * A[8]) * inv_det;
    invA[2] = (A[1] * A[5] - A[2] * A[4]) * inv_det;

    invA[3] = c01 * inv_det;
    invA[4] = (A[0] * A[8] - A[2] * A[6]) * inv_det;
    invA[5] = (A[2] * A[3] - A[0] * A[5]) * inv_det;

    invA[6] = c02 * inv_det;
    invA[7] = (A[1] * A[6] - A[0] * A[7]) * inv_det;
    invA[8] = (A[0] * A[4] - A[1] * A[3]) * inv_det;

    return 1;
}

static void symmetrize4(float P[16])
{
    float p01 = 0.5f * (P[1] + P[4]);
    float p02 = 0.5f * (P[2] + P[8]);
    float p03 = 0.5f * (P[3] + P[12]);
    float p12 = 0.5f * (P[6] + P[9]);
    float p13 = 0.5f * (P[7] + P[13]);
    float p23 = 0.5f * (P[11] + P[14]);

    P[1] = p01;
    P[4] = p01;

    P[2] = p02;
    P[8] = p02;

    P[3] = p03;
    P[12] = p03;

    P[6] = p12;
    P[9] = p12;

    P[7] = p13;
    P[13] = p13;

    P[11] = p23;
    P[14] = p23;
}

static int normalize_quaternion(kalman_state *kal)
{
    const float n2 = kal->qw * kal->qw + kal->qx * kal->qx + kal->qy * kal->qy + kal->qz * kal->qz;

    if (n2 < KALMAN_EPS)
    {
        kal->qw = 1.0f;
        kal->qx = 0.0f;
        kal->qy = 0.0f;
        kal->qz = 0.0f;
        return 0;
    }

    const float inv_n = 1.0f / sqrtf(n2);

    kal->qw *= inv_n;
    kal->qx *= inv_n;
    kal->qy *= inv_n;
    kal->qz *= inv_n;

    return 1;
}

void SPP_SERVICES_KALMAN_ekfInit(kalman_state *kal, sensor_data *data, float Pinit, const float *Q,
                                 const float *R)
{
    if ((kal == 0) || (data == 0))
    {
        return;
    }

    kal->qw = 1.0f;
    kal->qx = 0.0f;
    kal->qy = 0.0f;
    kal->qz = 0.0f;

    mat4_zero(kal->P);
    kal->P[0] = Pinit;
    kal->P[5] = Pinit;
    kal->P[10] = Pinit;
    kal->P[15] = Pinit;

    mat4_zero(kal->Q);

    if (Q != 0)
    {
        /*
         * Se asume Q como matriz 4x4 row-major.
         * Si no quieres pasar Q, pasa NULL y se calculará en predict.
         */
        kal->Q[0] = Q[0];
        kal->Q[5] = Q[5];
        kal->Q[10] = Q[10];
        kal->Q[15] = Q[15];
    }

    if (R != 0)
    {
        kal->R[0] = R[0];
        kal->R[1] = R[1];
        kal->R[2] = R[2];
    }
    else
    {
        kal->R[0] = 0.05f;
        kal->R[1] = 0.05f;
        kal->R[2] = 0.05f;
    }

    data->acc_old_data[0] = 0.0f;
    data->acc_old_data[1] = 0.0f;
    data->acc_old_data[2] = 0.0f;

    data->gyro_old_data[0] = 0.0f;
    data->gyro_old_data[1] = 0.0f;
    data->gyro_old_data[2] = 0.0f;

    data->acc_new_data = 0U;
    data->gyro_new_data = 0U;
}

void SPP_SERVICES_KALMAN_ekfPredict(kalman_state *kal, sensor_data *data, float T)
{
    if ((kal == 0) || (data == 0))
    {
        return;
    }

    if (T <= 0.0f)
    {
        return;
    }

    normalize_quaternion(kal);

    const float p = data->gyro_data[0];
    const float q = data->gyro_data[1];
    const float r = data->gyro_data[2];

    const float qw_old = kal->qw;
    const float qx_old = kal->qx;
    const float qy_old = kal->qy;
    const float qz_old = kal->qz;

    const float half_T = 0.5f * T;

    const float dp = half_T * p;
    const float dq = half_T * q;
    const float dr = half_T * r;

    /*
     * Propagación del cuaternión:
     * q_dot = 0.5 * q ⊗ [0, wx, wy, wz]
     */
    kal->qw = qw_old - dp * qx_old - dq * qy_old - dr * qz_old;
    kal->qx = qx_old + dp * qw_old - dq * qz_old + dr * qy_old;
    kal->qy = qy_old + dp * qz_old + dq * qw_old - dr * qx_old;
    kal->qz = qz_old - dp * qy_old + dq * qx_old + dr * qw_old;

    normalize_quaternion(kal);

    /*
     * Jacobiano F = df/dx.
     */
    float F[16] = {1.0f, -dp, -dq, -dr, dp, 1.0f, dr, -dq, dq, -dr, 1.0f, dp, dr, dq, -dp, 1.0f};

    /*
     * Ruido de proceso:
     * Q = W * Sigma_gyro * W'
     */
    const float half_Tqw = half_T * qw_old;
    const float half_Tqx = half_T * qx_old;
    const float half_Tqy = half_T * qy_old;
    const float half_Tqz = half_T * qz_old;

    float W[12];

    /*
     * W: 4x3
     */
    W[0] = -half_Tqx;
    W[1] = -half_Tqy;
    W[2] = -half_Tqz;

    W[3] = half_Tqw;
    W[4] = -half_Tqz;
    W[5] = half_Tqy;

    W[6] = half_Tqz;
    W[7] = half_Tqw;
    W[8] = -half_Tqx;

    W[9] = -half_Tqy;
    W[10] = half_Tqx;
    W[11] = half_Tqw;

    float W_sigma[12];

    for (int row = 0; row < 4; row++)
    {
        W_sigma[3 * row + 0] = W[3 * row + 0] * GYRO_X_VAR;
        W_sigma[3 * row + 1] = W[3 * row + 1] * GYRO_Y_VAR;
        W_sigma[3 * row + 2] = W[3 * row + 2] * GYRO_Z_VAR;
    }

    float WT[12];
    mat4x3_transpose_to_3x4(W, WT);

    mat4x3_mul_3x4(W_sigma, WT, kal->Q);

    /*
     * P = F * P * F' + Q
     */
    float FP[16];
    float FT[16];
    float FPFT[16];

    mat4_mul(F, kal->P, FP);
    mat4_transpose(F, FT);
    mat4_mul(FP, FT, FPFT);
    mat4_add(FPFT, kal->Q, kal->P);

    symmetrize4(kal->P);
}

void SPP_SERVICES_KALMAN_ekfUpdate(kalman_state *kal, sensor_data *data)
{
    if ((kal == 0) || (data == 0))
    {
        return;
    }

    normalize_quaternion(kal);

    /*
     * Acelerómetro. Se normaliza a módulo g para que el update use dirección
     * de gravedad y no el módulo, que suele contaminarse con aceleraciones lineales.
     */
    const float ax_raw = data->acc_data[0];
    const float ay_raw = data->acc_data[1];
    const float az_raw = data->acc_data[2];

    const float acc_norm2 = ax_raw * ax_raw + ay_raw * ay_raw + az_raw * az_raw;

    if (acc_norm2 < KALMAN_EPS)
    {
        return;
    }

    const float acc_scale = g / sqrtf(acc_norm2);

    const float ax = ax_raw * acc_scale;
    const float ay = ay_raw * acc_scale;
    const float az = az_raw * acc_scale;

    const float qw = kal->qw;
    const float qx = kal->qx;
    const float qy = kal->qy;
    const float qz = kal->qz;

    /*
     * h(x) = C' * [0, 0, g]
     *
     * Con q = [qw, qx, qy, qz]:
     *
     * h0 = 2g(qx qz - qw qy)
     * h1 = 2g(qy qz + qw qx)
     * h2 = g(qw² - qx² - qy² + qz²)
     */
    float h[3];

    h[0] = 2.0f * g * (qx * qz - qw * qy);
    h[1] = 2.0f * g * (qy * qz + qw * qx);
    h[2] = g * (qw * qw - qx * qx - qy * qy + qz * qz);

    /*
     * Jacobiano H = dh/dq.
     * H: 3x4 row-major.
     */
    float H[12];

    H[0] = -2.0f * g * qy;
    H[1] = 2.0f * g * qz;
    H[2] = -2.0f * g * qw;
    H[3] = 2.0f * g * qx;

    H[4] = 2.0f * g * qx;
    H[5] = 2.0f * g * qw;
    H[6] = 2.0f * g * qz;
    H[7] = 2.0f * g * qy;

    H[8] = 2.0f * g * qw;
    H[9] = -2.0f * g * qx;
    H[10] = -2.0f * g * qy;
    H[11] = 2.0f * g * qz;

    /*
     * Innovación v = z - h.
     */
    float v[3];

    v[0] = ax - h[0];
    v[1] = ay - h[1];
    v[2] = az - h[2];

    /*
     * S = H * P * H' + R
     */
    float HT[12];
    float PHt[12];
    float HPHt[9];
    float S[9];

    mat3x4_transpose_to_4x3(H, HT);
    mat4x4_mul_4x3(kal->P, HT, PHt);
    mat3x4_mul_4x3(H, PHt, HPHt);

    mat3_zero(S);

    S[0] = HPHt[0] + kal->R[0];
    S[1] = HPHt[1];
    S[2] = HPHt[2];

    S[3] = HPHt[3];
    S[4] = HPHt[4] + kal->R[1];
    S[5] = HPHt[5];

    S[6] = HPHt[6];
    S[7] = HPHt[7];
    S[8] = HPHt[8] + kal->R[2];

    float S_inv[9];

    if (!mat3_inverse(S, S_inv))
    {
        return;
    }

    /*
     * K = P * H' * inv(S)
     */
    float K[12];
    mat4x3_mul_3x3(PHt, S_inv, K);

    /*
     * x = x + K * v
     */
    float dx[4];
    mat4x3_mul_vec3(K, v, dx);

    kal->qw += dx[0];
    kal->qx += dx[1];
    kal->qy += dx[2];
    kal->qz += dx[3];

    normalize_quaternion(kal);

    /*
     * Actualización Joseph:
     *
     * P = (I - K H) P (I - K H)' + K R K'
     *
     * Es más estable numéricamente que:
     * P = (I - K H) P
     */
    float KH[16];
    float I[16];
    float A[16];

    mat4x3_mul_3x4(K, H, KH);
    mat4_identity(I);
    mat4_sub(I, KH, A);

    float AP[16];
    float AT[16];
    float APAT[16];

    mat4_mul(A, kal->P, AP);
    mat4_transpose(A, AT);
    mat4_mul(AP, AT, APAT);

    /*
     * KRK' con R diagonal.
     */
    float KR[12];

    for (int row = 0; row < 4; row++)
    {
        KR[3 * row + 0] = K[3 * row + 0] * kal->R[0];
        KR[3 * row + 1] = K[3 * row + 1] * kal->R[1];
        KR[3 * row + 2] = K[3 * row + 2] * kal->R[2];
    }

    float KT[12];
    float KRKT[16];

    mat4x3_transpose_to_3x4(K, KT);
    mat4x3_mul_3x4(KR, KT, KRKT);

    mat4_add(APAT, KRKT, kal->P);

    symmetrize4(kal->P);
}

void SPP_SERVICES_KALMAN_run(kalman_state *kal, sensor_data *data, float T)
{
    if ((kal == 0) || (data == 0))
    {
        return;
    }

    SPP_SERVICES_KALMAN_newDataCheck(data);

    if (data->gyro_new_data != 0U)
    {
        SPP_SERVICES_KALMAN_ekfPredict(kal, data, T);

        data->gyro_old_data[0] = data->gyro_data[0];
        data->gyro_old_data[1] = data->gyro_data[1];
        data->gyro_old_data[2] = data->gyro_data[2];

        data->gyro_new_data = 0U;
    }

    if (data->acc_new_data != 0U)
    {
        SPP_SERVICES_KALMAN_ekfUpdate(kal, data);

        data->acc_old_data[0] = data->acc_data[0];
        data->acc_old_data[1] = data->acc_data[1];
        data->acc_old_data[2] = data->acc_data[2];

        data->acc_new_data = 0U;
    }
}

void SPP_SERVICES_KALMAN_newDataCheck(sensor_data *data)
{
    if (data == 0)
    {
        return;
    }

    data->acc_new_data = 0U;
    data->gyro_new_data = 0U;

    if ((fabsf(data->acc_data[0] - data->acc_old_data[0]) > SENSOR_DATA_TOL) ||
        (fabsf(data->acc_data[1] - data->acc_old_data[1]) > SENSOR_DATA_TOL) ||
        (fabsf(data->acc_data[2] - data->acc_old_data[2]) > SENSOR_DATA_TOL))
    {
        data->acc_new_data = 1U;
    }

    if ((fabsf(data->gyro_data[0] - data->gyro_old_data[0]) > SENSOR_DATA_TOL) ||
        (fabsf(data->gyro_data[1] - data->gyro_old_data[1]) > SENSOR_DATA_TOL) ||
        (fabsf(data->gyro_data[2] - data->gyro_old_data[2]) > SENSOR_DATA_TOL))
    {
        data->gyro_new_data = 1U;
    }
}