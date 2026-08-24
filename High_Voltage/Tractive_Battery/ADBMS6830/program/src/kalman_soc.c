/**
 * @file kalman_soc_flash.cpp
 * @brief EXACT MATLAB port with R/C lookups, SOH, and flash persistence
 * 
 * Changes from base version:
 * 1. R_ct, C_ct, R_dif, C_dif now use 2D lookup tables (SOC × Temperature)
 * 2. SOH calculation based on R0 vs SOC comparison
 * 3. Flash storage for state persistence across power cycles
 */

#include "kalman_soc.h"
#include "battery_model.h"
#include "kalman_soc_config.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// CRC32 TABLE (for flash validation)
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
	 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
	 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
	 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
	 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
	 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
	 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
	 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
	 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
	 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
	 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
	 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	 0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
	 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
	 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
	 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
	 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
	 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
	 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
	 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
	 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
	 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static uint32_t calculate_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// 2D INTERPOLATION (bilinear)
// ============================================================================

static float interp2d(const RC_LookupTable *lut, float soc, float temp)
{
    // Clamp inputs to table bounds
    if (soc < lut->soc_points[0]) soc = lut->soc_points[0];
    if (soc > lut->soc_points[RC_LUT_SOC_SIZE - 1]) soc = lut->soc_points[RC_LUT_SOC_SIZE - 1];
    if (temp < lut->temp_points[0]) temp = lut->temp_points[0];
    if (temp > lut->temp_points[RC_LUT_TEMP_SIZE - 1]) temp = lut->temp_points[RC_LUT_TEMP_SIZE - 1];
    
    // Find SOC indices
    int soc_idx = 0;
    for (int i = 0; i < RC_LUT_SOC_SIZE - 1; i++) {
        if (soc >= lut->soc_points[i] && soc <= lut->soc_points[i + 1]) {
            soc_idx = i;
            break;
        }
    }
    
    // Find temperature indices
    int temp_idx = 0;
    for (int i = 0; i < RC_LUT_TEMP_SIZE - 1; i++) {
        if (temp >= lut->temp_points[i] && temp <= lut->temp_points[i + 1]) {
            temp_idx = i;
            break;
        }
    }
    
    // Bilinear interpolation
    float soc1 = lut->soc_points[soc_idx];
    float soc2 = lut->soc_points[soc_idx + 1];
    float temp1 = lut->temp_points[temp_idx];
    float temp2 = lut->temp_points[temp_idx + 1];
    
    float v11 = lut->values[temp_idx][soc_idx];
    float v12 = lut->values[temp_idx + 1][soc_idx];
    float v21 = lut->values[temp_idx][soc_idx + 1];
    float v22 = lut->values[temp_idx + 1][soc_idx + 1];
    
    float w_soc = (soc - soc1) / (soc2 - soc1);
    float w_temp = (temp - temp1) / (temp2 - temp1);
    
    float v1 = v11 * (1.0f - w_temp) + v12 * w_temp;
    float v2 = v21 * (1.0f - w_temp) + v22 * w_temp;
    
    return v1 * (1.0f - w_soc) + v2 * w_soc;
}

// ============================================================================
// 1D INTERPOLATION (linear)
// ============================================================================

static float interp1d(const float *x, const float *y, uint16_t size, float xi)
{
    if (xi <= x[0]) return y[0];
    if (xi >= x[size - 1]) return y[size - 1];
    
    for (uint16_t i = 0; i < size - 1; i++) {
        if (xi >= x[i] && xi <= x[i + 1]) {
            float w = (xi - x[i]) / (x[i + 1] - x[i]);
            return y[i] * (1.0f - w) + y[i + 1] * w;
        }
    }
    
    return y[size - 1];
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void KalmanSOC_Init(KalmanSOC *kf, float initial_soc)
{
    // MATLAB: obj.x = [0.3;0;0];
    kf->x.soc = (initial_soc >= 0.0f) ? initial_soc : 0.3f;
    kf->x.v_ct = 0.0f;
    kf->x.v_dif = 0.0f;
    
    // MATLAB: obj.theta = [obj.r0;obj.q_nom];
    kf->theta.r0 = INITIAL_R0;
    kf->theta.q_nom = INITIAL_QNOM;
    
    // MATLAB: obj.p = [1e-3 0 0;0 1e-3 0;0 0 1e-3];
    kf->P.data[0][0] = 1e-3f;
    kf->P.data[0][1] = 0.0f;
    kf->P.data[0][2] = 0.0f;
    kf->P.data[1][0] = 0.0f;
    kf->P.data[1][1] = 1e-3f;
    kf->P.data[1][2] = 0.0f;
    kf->P.data[2][0] = 0.0f;
    kf->P.data[2][1] = 0.0f;
    kf->P.data[2][2] = 1e-3f;
    
    // MATLAB: obj.p_theta = [0.1 0; 0 0.001];
    kf->P_theta.data[0][0] = 0.1f;
    kf->P_theta.data[0][1] = 0.0f;
    kf->P_theta.data[1][0] = 0.0f;
    kf->P_theta.data[1][1] = 0.001f;
    
    // MATLAB: obj.k = zeros(3,2);
    memset(kf->k, 0, sizeof(kf->k));
    
    // MATLAB: obj.k_theta = [1 1;1 1];
    kf->k_theta[0][0] = 1.0f;
    kf->k_theta[0][1] = 1.0f;
    kf->k_theta[1][0] = 1.0f;
    kf->k_theta[1][1] = 1.0f;
    
    // MATLAB: obj.c_theta = [0 0;0 0];
    kf->c_theta[0][0] = 0.0f;
    kf->c_theta[0][1] = 0.0f;
    kf->c_theta[1][0] = 0.0f;
    kf->c_theta[1][1] = 0.0f;
    
    // MATLAB: obj.q_fixed initialization
    kf->q_fixed[0][0] = Q_SOC;
    kf->q_fixed[0][1] = 0.0f;
    kf->q_fixed[0][2] = 0.0f;
    kf->q_fixed[1][0] = 0.0f;
    kf->q_fixed[1][1] = Q_VCT;
    kf->q_fixed[1][2] = 0.0f;
    kf->q_fixed[2][0] = 0.0f;
    kf->q_fixed[2][1] = 0.0f;
    kf->q_fixed[2][2] = Q_VDIF;
    
    // MATLAB: obj.d_window = zeros(1,obj.N);
    memset(kf->innovation_window, 0, sizeof(kf->innovation_window));
    
    // MATLAB: obj.I_prev = 0;
    kf->i_prev = 0.0f;
    
    // MATLAB: obj.q_prev = obj.q_nom;
    kf->q_prev = kf->theta.q_nom;
    
    // MATLAB: obj.dx_dtheta = zeros(3,2);
    memset(kf->dx_dtheta, 0, sizeof(kf->dx_dtheta));
    
    // MATLAB: obj.t = 0;
    kf->t = 0.0f;
    
    // MATLAB: obj.z = 0.3;
    kf->z = 0.3f;
    
    // NEW: SOH initialization
    kf->soh_est = 1.0f;  // 100% SOH initially
    
    // NEW: R/C lookup tables (NULL until set)
    /*
    kf->r_ct_lut = NULL;
    kf->c_ct_lut = NULL;
    kf->r_dif_lut = NULL;
    kf->c_dif_lut = NULL;
    
    // NEW: R0 lookup (NULL until set)
    kf->r0_soc_points = NULL;
    kf->r0_values = NULL;
    kf->r0_lut_size = 0;
    */
    kf->t = 0.0f;
    
    kf->window_index = 0;
    kf->initialized = true;
}

void KalmanSOC_InitFromVoltage(KalmanSOC *kf, float rest_voltage, float temp_c)
{
    float initial_soc = BatteryModel_GetSocFromOcv(rest_voltage);
    KalmanSOC_Init(kf, initial_soc);
}

bool KalmanSOC_InitFromFlash(KalmanSOC *kf, const KalmanSOC_PersistentState *persistent)
{
    // Validate persistent state
    if (!KalmanSOC_ValidatePersistentState(persistent)) {
        return false;
    }
    
    // Initialize with default first
    KalmanSOC_Init(kf, persistent->soc);
    
    // Restore state
    kf->x.soc = persistent->soc;
    kf->x.v_ct = persistent->v_ct;
    kf->x.v_dif = persistent->v_dif;
    
    // Restore parameters
    kf->theta.r0 = persistent->r0;
    kf->theta.q_nom = persistent->q_nom;
    
    // Restore covariances
    memcpy(kf->P.data, persistent->P, sizeof(persistent->P));
    memcpy(kf->P_theta.data, persistent->P_theta, sizeof(persistent->P_theta));
    
    // Restore SOH
    kf->soh_est = persistent->soh_est;
    
    return true;
}

void KalmanSOC_SetRCLookupTables(KalmanSOC *kf,
                                  const RC_LookupTable *r_ct,
                                  const RC_LookupTable *c_ct,
                                  const RC_LookupTable *r_dif,
                                  const RC_LookupTable *c_dif,
								  const RC_LookupTable *r_0)
{
    kf->r_ct_lut = r_ct;
    kf->c_ct_lut = c_ct;
    kf->r_dif_lut = r_dif;
    kf->c_dif_lut = c_dif;
    kf->g_r0_lut = r_0;
}

// ============================================================================
// FLASH PERSISTENCE
// ============================================================================

void KalmanSOC_GetPersistentState(const KalmanSOC *kf, KalmanSOC_PersistentState *persistent)
{
    persistent->soc = kf->x.soc;
    persistent->v_ct = kf->x.v_ct;
    persistent->v_dif = kf->x.v_dif;
    
    persistent->r0 = kf->theta.r0;
    persistent->q_nom = kf->theta.q_nom;
    
    memcpy(persistent->P, kf->P.data, sizeof(persistent->P));
    memcpy(persistent->P_theta, kf->P_theta.data, sizeof(persistent->P_theta));
    
    persistent->soh_est = kf->soh_est;
    
    persistent->magic_number = KALMAN_MAGIC_NUMBER;
    
    // Calculate CRC (excluding the CRC field itself)
    persistent->crc32 = calculate_crc32((const uint8_t*)persistent, 
                                        sizeof(KalmanSOC_PersistentState) - sizeof(uint32_t));
}

uint32_t KalmanSOC_CalculateCRC32(const KalmanSOC_PersistentState *persistent)
{
    return calculate_crc32((const uint8_t*)persistent, 
                          sizeof(KalmanSOC_PersistentState) - sizeof(uint32_t));
}

bool KalmanSOC_ValidatePersistentState(const KalmanSOC_PersistentState *persistent)
{
    // Check magic number
    if (persistent->magic_number != KALMAN_MAGIC_NUMBER) {
        return false;
    }
    
    // Check CRC
    uint32_t calculated_crc = KalmanSOC_CalculateCRC32(persistent);
    if (calculated_crc != persistent->crc32) {
        return false;
    }
    
    // Sanity checks
    if (persistent->soc < 0.0f || persistent->soc > 1.0f) return false;
    if (persistent->r0 < 0.0f || persistent->r0 > 1.0f) return false;
    if (persistent->q_nom < 0.5f * INITIAL_QNOM || persistent->q_nom > 2.0f * INITIAL_QNOM) return false;
    if (persistent->soh_est < 0.5f || persistent->soh_est > 1.5f) return false;
    
    return true;
}

// Continued in next file...
/**
 * @file kalman_soc_flash_part2.cpp
 * @brief Main KalmanSOC_Update with R/C lookups and SOH calculation
 * 
 * Append this to part1 to create complete kalman_soc.cpp
 */

// ============================================================================
// MAIN UPDATE FUNCTION (WITH R/C LOOKUPS AND SOH)
// ============================================================================

bool KalmanSOC_Update(KalmanSOC *kf, const SOC_Measurement *meas, SOC_Estimate *estimate)
{
    float V = meas->voltage;
    float i = meas->current;
    float T = meas->temperature;
    float t_new = meas->timestamp_ms / 1000.0f;
    
    // Place this near the top of the update function
    float dt = t_new - kf->t;
    float current_derivative = 0.0f;
    if (dt > 1e-5f) {
        current_derivative = (i - kf->i_prev) / dt;
    }

    // Get R/C values from lookup tables (temperature compensated)
    float R_ct, C_ct, R_dif, C_dif;
    
    if (kf->r_ct_lut != NULL && kf->c_ct_lut != NULL && 
        kf->r_dif_lut != NULL && kf->c_dif_lut != NULL) {
        // Use 2D lookups (SOC × Temperature)
        R_ct = interp2d(kf->r_ct_lut, kf->x.soc, T);
        C_ct = interp2d(kf->c_ct_lut, kf->x.soc, T);
        R_dif = interp2d(kf->r_dif_lut, kf->x.soc, T);
        C_dif = interp2d(kf->c_dif_lut, kf->x.soc, T);
    } else {
        // Fallback to fixed values if lookups not set
        R_ct = 0.003f;
        C_ct = 1000.0f;
        R_dif = 0.006f;
        C_dif = 100000.0f;
    }
    
    // MATLAB: obj.a = [1 0 0;0 exp(-(t_new-obj.t)/(R_ct*C_ct)) 0; 0 0 exp(-(t_new-obj.t)/(R_dif*C_dif))];
    float a[3][3];
    a[0][0] = 1.0f;
    a[0][1] = 0.0f;
    a[0][2] = 0.0f;
    a[1][0] = 0.0f;
    a[1][1] = expf(-(t_new - kf->t) / (R_ct * C_ct));
    a[1][2] = 0.0f;
    a[2][0] = 0.0f;
    a[2][1] = 0.0f;
    a[2][2] = expf(-(t_new - kf->t) / (R_dif * C_dif));
    
    float current_q_r0 = Q_R0;
	float current_q_qnom = Q_QNOM;

	if (fabsf(current_derivative) > MAX_ALLOWED_DI_DT) {
		// Clamp parameter process noise to zero to freeze theta adaptation during transients
		current_q_r0 = 0.0f;
		current_q_qnom = 0.0f;
	}
    // MATLAB: obj.q_theta_fixed = [1e-8 0; 0 1e-12];
    float q_theta_fixed[2][2];
    q_theta_fixed[0][0] = current_q_r0;
    q_theta_fixed[0][1] = 0.0f;
    q_theta_fixed[1][0] = 0.0f;
    q_theta_fixed[1][1] = current_q_qnom;
    
    // MATLAB: q = obj.q_fixed;
    float q[3][3];
    memcpy(q, kf->q_fixed, sizeof(q));
    
    // MATLAB: obj.theta = obj.predTheta(obj.theta);
    // (theta stays same in prediction)
    
    // MATLAB: obj.p_theta = obj.predPTheta(obj.p_theta, obj.q_theta_fixed);
    kf->P_theta.data[0][0] += q_theta_fixed[0][0];
    kf->P_theta.data[0][1] += q_theta_fixed[0][1];
    kf->P_theta.data[1][0] += q_theta_fixed[1][0];
    kf->P_theta.data[1][1] += q_theta_fixed[1][1];
    
    // MATLAB: obj.b = [(t_new-obj.t)/obj.theta(2); -1*R_ct*(1-exp(...)); -1*R_dif*(1-exp(...))];
    float b[3];
    b[0] = (t_new - kf->t) / kf->theta.q_nom;
    b[1] = -1.0f * R_ct * (1.0f - expf(-(t_new - kf->t) / (R_ct * C_ct)));
    b[2] = -1.0f * R_dif * (1.0f - expf(-(t_new - kf->t) / (R_dif * C_dif)));
    
    // MATLAB: obj.x = obj.predX(obj.a,obj.x,obj.b,obj.I_prev);
    float x_pred[3];
    x_pred[0] = a[0][0] * kf->x.soc + a[0][1] * kf->x.v_ct + a[0][2] * kf->x.v_dif - b[0] * kf->i_prev;
    x_pred[1] = a[1][0] * kf->x.soc + a[1][1] * kf->x.v_ct + a[1][2] * kf->x.v_dif + b[1] * kf->i_prev;
    x_pred[2] = a[2][0] * kf->x.soc + a[2][1] * kf->x.v_ct + a[2][2] * kf->x.v_dif + b[2] * kf->i_prev;
    
    kf->x.soc = x_pred[0];
    kf->x.v_ct = x_pred[1];
    kf->x.v_dif = x_pred[2];
    
    // MATLAB: obj.p = obj.predP(obj.a,obj.p,q);
    float ap[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            ap[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                ap[i][j] += a[i][k] * kf->P.data[k][j];
            }
        }
    }
    
    float apat[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            apat[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                apat[i][j] += ap[i][k] * a[j][k];
            }
        }
    }
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kf->P.data[i][j] = apat[i][j] + q[i][j];
        }
    }

    // MATLAB: Extrapolation logic (changed from old version)
    //float extrap_ocv, extrap_ke, extrap_soc;

    float ocv_val = BatteryModel_GetOcv(kf->x.soc);
    
    // MATLAB: obj.x(1) = max(0, min(obj.x(1),1));
    if (kf->x.soc < 0.0f) kf->x.soc = 0.0f;
    if (kf->x.soc > 1.0f) kf->x.soc = 1.0f;
    
    // MATLAB: obj.c = [obj.ke(obj.x(1),extrap_ke) -1 -1;1 0 0];
    float c[2][3];
    c[0][0] = BatteryModel_GetSlope(kf->x.soc);
    c[0][1] = -1.0f;
    c[0][2] = -1.0f;
    c[1][0] = 1.0f;
    c[1][1] = 0.0f;
    c[1][2] = 0.0f;
    
    // MATLAB: obj.z = double(obj.ocv_inv(V - obj.theta(1)*i,extrap_soc));
    kf->z = BatteryModel_GetSocFromOcv(V - kf->theta.r0 * i);
    if (kf->z < 0.0f) kf->z = 0.0f;
    if (kf->z > 1.0f) kf->z = 1.0f;
    
    // MATLAB: v = obj.g(obj.x, obj.ocv(obj.x(1),extrap_ocv), i, obj.theta(1));
    float v[2];
    v[0] = ocv_val - kf->x.v_ct - kf->x.v_dif - kf->theta.r0 * i;
    v[1] = kf->x.soc;
    
    // MATLAB: obj.dx_dtheta = ... (sensitivity update)
    float df_dtheta[3][2];
    df_dtheta[0][0] = 0.0f;
    df_dtheta[0][1] = (-(t_new - kf->t) * kf->i_prev) / (kf->q_prev * kf->q_prev);
    df_dtheta[1][0] = 0.0f;
    df_dtheta[1][1] = 0.0f;
    df_dtheta[2][0] = 0.0f;
    df_dtheta[2][1] = 0.0f;
    
    float k_ctheta[3][2];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            k_ctheta[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                k_ctheta[i][j] += kf->k[i][k] * kf->c_theta[k][j];
            }
        }
    }
    
    float dx_minus_kc[3][2];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            dx_minus_kc[i][j] = kf->dx_dtheta[i][j] - k_ctheta[i][j];
        }
    }
    
    float a_dx[3][2];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            a_dx[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                a_dx[i][j] += a[i][k] * dx_minus_kc[k][j];
            }
        }
    }
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            kf->dx_dtheta[i][j] = df_dtheta[i][j] + a_dx[i][j];
        }
    }
    
    // MATLAB: obj.c_theta = double([i 0;0 0] + obj.c*(...));
    float c_dx[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            c_dx[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                c_dx[i][j] += c[i][k] * (df_dtheta[k][j] + kf->dx_dtheta[k][j]);
            }
        }
    }
    
    kf->c_theta[0][0] = i + c_dx[0][0];
    kf->c_theta[0][1] = 0.0f + c_dx[0][1];
    kf->c_theta[1][0] = 0.0f + c_dx[1][0];
    kf->c_theta[1][1] = 0.0f + c_dx[1][1];
    
    // MATLAB: obj.q_prev = obj.theta(2);
    kf->q_prev = kf->theta.q_nom;
    
    // MATLAB: obj.d = V - v(1);
    float d = V - v[0];
    
    if (fabsf(d) > MAX_PHYSICAL_INNOVATION_V) {
		// 1. Bypass correction: Skip Kalman gain updates and keep the a priori prediction
		// 2. Clear out the innovation window history so the adaptive Q matrix doesn't balloon
		for (int idx = 0; idx < INNOVATION_WINDOW_SIZE; idx++) {
			kf->innovation_window[idx] = 0.0f;
		}

		// 3. Save the timeline tracking variables so the next loop has a valid dt base
		kf->i_prev = i;
		kf->t = t_new;

		// 4. Fill output structure using the prediction state and return early safely
		estimate->soc = kf->x.soc;
		estimate->soc_percent = kf->x.soc * 100.0f;
		estimate->voltage_ocv = ocv_val;
		estimate->v_ct = kf->x.v_ct;
		estimate->v_dif = kf->x.v_dif;
		estimate->r0 = kf->theta.r0;
		estimate->capacity_ah = kf->theta.q_nom / 3600.0f;
		estimate->soh_percent = kf->soh_est * 100.0f;
		estimate->valid = true;
		estimate->confidence = sqrt(fabsf(kf->P.data[1][1]));
		return true;
	}
    // MATLAB: obj.I_prev = double(i);
    kf->i_prev = i;
    
    // MATLAB: obj.t = t_new;
    kf->t = t_new;
    
    // MATLAB: State update (K computation and application)
    float r[2][2];
    r[0][0] = R_VOLTAGE;
    r[0][1] = 0.0f;
    r[1][0] = 0.0f;
    r[1][1] = R_SOC_PSEUDO;
    
    float pct[3][2];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            pct[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                pct[i][j] += kf->P.data[i][k] * c[j][k];
            }
        }
    }
    
    float cpct[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            cpct[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                cpct[i][j] += c[i][k] * pct[k][j];
            }
        }
    }
    
    float s[2][2];
    s[0][0] = cpct[0][0] + r[0][0];
    s[0][1] = cpct[0][1] + r[0][1];
    s[1][0] = cpct[1][0] + r[1][0];
    s[1][1] = cpct[1][1] + r[1][1];
    
    float det = s[0][0] * s[1][1] - s[0][1] * s[1][0];
    if (fabsf(det) < 1e-10f) return false;
    
    float s_inv[2][2];
    s_inv[0][0] = s[1][1] / det;
    s_inv[0][1] = -s[0][1] / det;
    s_inv[1][0] = -s[1][0] / det;
    s_inv[1][1] = s[0][0] / det;
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            kf->k[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                kf->k[i][j] += pct[i][k] * s_inv[k][j];
            }
        }
    }
    
    // MATLAB: obj.x = double(obj.updateX(...));
    float z_vec[2];
    z_vec[0] = V;
    z_vec[1] = kf->z;
    
    float innov[2];
    innov[0] = z_vec[0] - v[0];
    innov[1] = z_vec[1] - v[1];
    
    kf->x.soc = kf->x.soc + kf->k[0][0] * innov[0] + kf->k[0][1] * innov[1];
    kf->x.v_ct = kf->x.v_ct + kf->k[1][0] * innov[0] + kf->k[1][1] * innov[1];
    kf->x.v_dif = kf->x.v_dif + kf->k[2][0] * innov[0] + kf->k[2][1] * innov[1];
    
    // MATLAB: obj.p = obj.updateP(...);
    float kc[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kc[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                kc[i][j] += kf->k[i][k] * c[k][j];
            }
        }
    }
    
    float i_kc[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            i_kc[i][j] = (i == j ? 1.0f : 0.0f) - kc[i][j];
        }
    }
    
    float p_new[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            p_new[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                p_new[i][j] += i_kc[i][k] * kf->P.data[k][j];
            }
        }
    }
    memcpy(kf->P.data, p_new, sizeof(p_new));

    if (kf->x.soc < 0.0f) kf->x.soc = 0.0f;
    if (kf->x.soc > 1.0f) kf->x.soc = 1.0f;

    // MATLAB: Innovation window update (adaptive Q)
    for (int i = 0; i < INNOVATION_WINDOW_SIZE - 1; i++) {
        kf->innovation_window[i] = kf->innovation_window[i + 1];
    }
    kf->innovation_window[INNOVATION_WINDOW_SIZE - 1] = d;
    
    float sum = 0.0f;
    for (int i = 0; i < INNOVATION_WINDOW_SIZE; i++) {
        sum += kf->innovation_window[i] * kf->innovation_window[i];
    }
    float D = sum / INNOVATION_WINDOW_SIZE;
    
    // Calculate the raw adaptive variance contribution (K * D * K')
	// We only compute the diagonal elements to maintain filter stability
	// and prevent unmodeled cross-correlations from causing divergence.
	float q_adaptive_soc  = kf->k[0][0] * D * kf->k[0][0] + kf->k[0][1] * D * kf->k[0][1];
	float q_adaptive_vct  = kf->k[1][0] * D * kf->k[1][0] + kf->k[1][1] * D * kf->k[1][1];
	float q_adaptive_vdif = kf->k[2][0] * D * kf->k[2][0] + kf->k[2][1] * D * kf->k[2][1];

	// Enforce strict upper ceilings on how much noise the filter can adaptively add
	// This prevents the filter from entering a runaway feedback loop under heavy load
	if (q_adaptive_soc  > 1e-5f)  q_adaptive_soc  = 1e-5f;
	if (q_adaptive_vct  > 1e-3f)  q_adaptive_vct  = 1e-3f;
	if (q_adaptive_vdif > 1e-4f)  q_adaptive_vdif = 1e-4f;

	// Clear the matrix and apply baseline constants + bounded adaptive adjustments
	memset(kf->q_fixed, 0, sizeof(kf->q_fixed));
	kf->q_fixed[0][0] = Q_SOC  + q_adaptive_soc;
	kf->q_fixed[1][1] = Q_VCT  + q_adaptive_vct;
	kf->q_fixed[2][2] = Q_VDIF + q_adaptive_vdif;
    /*
    // MATLAB: obj.q_fixed = obj.k*obj.D*obj.k';
    float k_D[3][2];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            k_D[i][j] = kf->k[i][j] * D;
        }
    }
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kf->q_fixed[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                kf->q_fixed[i][j] += k_D[i][k] * kf->k[j][k];
            }
        }
    }
    */
    // MATLAB: Parameter update
    float r_theta[2][2];
    r_theta[0][0] = R_PARAM_R0;
    r_theta[0][1] = 0.0f;
    r_theta[1][0] = 0.0f;
    r_theta[1][1] = R_PARAM_QNOM;
    
    float p_theta_ct[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            p_theta_ct[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                p_theta_ct[i][j] += kf->P_theta.data[i][k] * kf->c_theta[j][k];
            }
        }
    }
    
    float c_theta_p_ct[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            c_theta_p_ct[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                c_theta_p_ct[i][j] += kf->c_theta[i][k] * p_theta_ct[k][j];
            }
        }
    }
    
    float s_theta[2][2];
    s_theta[0][0] = c_theta_p_ct[0][0] + r_theta[0][0];
    s_theta[0][1] = c_theta_p_ct[0][1] + r_theta[0][1];
    s_theta[1][0] = c_theta_p_ct[1][0] + r_theta[1][0];
    s_theta[1][1] = c_theta_p_ct[1][1] + r_theta[1][1];
    
    det = s_theta[0][0] * s_theta[1][1] - s_theta[0][1] * s_theta[1][0];
    if (fabsf(det) < 1e-10f) return false;
    
    float s_theta_inv[2][2];
    s_theta_inv[0][0] = s_theta[1][1] / det;
    s_theta_inv[0][1] = -s_theta[0][1] / det;
    s_theta_inv[1][0] = -s_theta[1][0] / det;
    s_theta_inv[1][1] = s_theta[0][0] / det;
    
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            kf->k_theta[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                kf->k_theta[i][j] += p_theta_ct[i][k] * s_theta_inv[k][j];
            }
        }
    }
    
    // MATLAB: obj.theta = double(obj.updateX(...));
    kf->theta.r0 = kf->theta.r0 + kf->k_theta[0][0] * innov[0] + kf->k_theta[0][1] * innov[1];
    kf->theta.q_nom = kf->theta.q_nom + kf->k_theta[1][0] * innov[0] + kf->k_theta[1][1] * innov[1];
    
    // MATLAB: obj.p_theta = obj.updateP(...);
    float k_theta_c[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            k_theta_c[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                k_theta_c[i][j] += kf->k_theta[i][k] * kf->c_theta[k][j];
            }
        }
    }
    
    float i_kc_theta[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            i_kc_theta[i][j] = (i == j ? 1.0f : 0.0f) - k_theta_c[i][j];
        }
    }
    
    float p_theta_new[2][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            p_theta_new[i][j] = 0.0f;
            for (int k = 0; k < 2; k++) {
                p_theta_new[i][j] += i_kc_theta[i][k] * kf->P_theta.data[k][j];
            }
        }
    }
    memcpy(kf->P_theta.data, p_theta_new, sizeof(p_theta_new));
    
    // MATLAB: obj.x(1) = max(0, obj.x(1));
    if (kf->x.soc < 0.0f) kf->x.soc = 0.0f;
    
    // MATLAB: obj.theta(1) = max(0, obj.theta(1));
    if (kf->theta.r0 < 0.0f) kf->theta.r0 = 0.0f;
    
    // ========================================================================
    // NEW: SOH CALCULATION (from SOH.m)
    // ========================================================================
    
    // MATLAB: if soc > 0.9
    if (kf->x.soc > SOH_UPDATE_SOC_THRESHOLD) {
        // r0_temp = interp1(r0_dat(:,1), r0_dat(:,2), soc, 'linear', 'extrap');
        float r0_expected = interp2d(kf->g_r0_lut, kf->x.soc,T);
        
        // soh_est = 1 + (r0 - r0_temp)/r0_temp;
        kf->soh_est = 1.0f + (kf->theta.r0 - r0_expected) / r0_expected;
        
        // Clamp SOH to reasonable range
        if (kf->soh_est < 0.5f) kf->soh_est = 0.5f;
        if (kf->soh_est > 1.5f) kf->soh_est = 1.5f;
    }
    
    // Output
    estimate->soc = kf->x.soc;
    estimate->soc_percent = kf->x.soc * 100.0f;
    estimate->voltage_ocv = BatteryModel_GetOcv(kf->x.soc);
    estimate->v_ct = kf->x.v_ct;
    estimate->v_dif = kf->x.v_dif;
    estimate->r0 = kf->theta.r0;
    estimate->capacity_ah = kf->theta.q_nom / 3600.0f;
    estimate->soh_percent = kf->soh_est * 100.0f;
    estimate->valid = true;
    estimate->confidence = sqrt(fabsf(kf->P.data[1][1]));
    
    return true;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void KalmanSOC_Reset(KalmanSOC *kf)
{
    KalmanSOC_Init(kf, kf->x.soc);
}

void KalmanSOC_GetEstimate(const KalmanSOC *kf, SOC_Estimate *estimate)
{
    estimate->soc = kf->x.soc;
    estimate->soc_percent = kf->x.soc * 100.0f;
    estimate->voltage_ocv = BatteryModel_GetOcv(kf->x.soc);
    estimate->v_ct = kf->x.v_ct;
    estimate->v_dif = kf->x.v_dif;
    estimate->r0 = kf->theta.r0;
    estimate->capacity_ah = kf->theta.q_nom / 3600.0f;
    estimate->soh_percent = kf->soh_est * 100.0f;
    estimate->valid = kf->initialized;
    estimate->confidence = 1.0f;
}

bool KalmanSOC_IsHealthy(const KalmanSOC *kf)
{
    return kf->initialized;
}

void KalmanSOC_GetStats(const KalmanSOC *kf, float *max_time_us, float *avg_time_us)
{
    *max_time_us = 0.0f;
    *avg_time_us = 0.0f;
}
