/**
 * @file sil_harness.c
 * @brief Software-in-the-loop driver for the real embedded KalmanSOC / BatteryModel code.
 *
 * This file is NEW glue code. It does not modify kalman_soc.c/.h, battery_model.c/.h,
 * kalman_soc_config.h, or common_types.h in any way -- those live in ../c_src, copied
 * verbatim (byte-for-byte) from High_Voltage/Tractive Battery/ADBMS6830/program.
 *
 * It reproduces exactly the same initialization and update call sequence the firmware
 * uses in ADBMS6830/program/src/adBms_Application.c
 * (see adBms6830_soc_init() / adBms6830_soc_update()):
 *
 *   1. KalmanSOC_InitFromVoltage() seeded from the first CSV row's voltage/temperature,
 *      exactly like the firmware seeding SOC from the first BMS voltage reading at boot.
 *   2. KalmanSOC_SetRCLookupTables() with the same R/C lookup tables (values copied
 *      verbatim from adBms_Application.c -- these are HPPC-test-derived constants, not
 *      something this harness invents).
 *   3. One KalmanSOC_Update() call per input CSV row, in file order, using that row's own
 *      timestamp -- exactly like the firmware calling adBms6830_soc_update() once per new
 *      voltage/current reading.
 *
 * Usage:
 *   soc_sil <input.csv> <output.csv>
 *
 * Input CSV: a header row followed by data rows containing (any column order):
 *   time_ms      milliseconds since the start of the log (matches HAL_GetTick() semantics)
 *   voltage      single-cell terminal voltage, volts
 *   current      pack current, amps, positive = discharge (matches kalman_soc.c's sign
 *                convention: SOC decreases when current is positive)
 *   temperature  optional, deg C -- defaults to 25.0 for every row if the column is absent
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "kalman_soc.h"
#include "battery_model.h"

/* The logger samples at 500 Hz (2 ms/row), but main.c on the real vehicle only calls
 * adBms6830_soc_update() at 1 Hz -- NOT the 10 Hz implied by kalman_soc_config.h's
 * SOC_UPDATE_PERIOD_MS (that constant is unused inside kalman_soc.c itself; nothing in
 * the estimator enforces a call rate, whatever calls KalmanSOC_Update() is responsible
 * for pacing it). To match the real vehicle instead of the config header's aspirational
 * value, this harness decimates to 1 Hz before calling KalmanSOC_Update(), so every call
 * corresponds to one real update tick -- exactly like the firmware -- instead of feeding
 * the parameter estimator 500x too many correlated samples per second. */
#define SIL_UPDATE_PERIOD_MS 1000.0f

/* ============================================================================
 * R/C lookup tables -- copied verbatim (identical values) from
 * High_Voltage/Tractive Battery/ADBMS6830/program/src/adBms_Application.c, where the
 * real firmware defines these and feeds them into KalmanSOC_SetRCLookupTables().
 * ========================================================================== */

static RC_LookupTable g_r_ct_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,
         0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,
         0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f,
         0.004822437902900810f},{0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,
                 0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,
                 0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f,
                 0.004822437902900810f}
    }
};

static RC_LookupTable g_c_ct_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f,
         5634.523954468950f, 5866.549079080130f, 5319.9156924664600f,
         7077.136410907460f, 5762.106628957510f, 3031.838226611050f,
         1451.5479806985900f},{3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f,
                 5634.523954468950f, 5866.549079080130f, 5319.9156924664600f,
                 7077.136410907460f, 5762.106628957510f, 3031.838226611050f,
                 1451.5479806985900f}
    }
};

static RC_LookupTable g_r_dif_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f,
         0.00125871530333011f, 0.0014997750862781800f, 0.001823055274690330f,
         0.001862907879059510f, 0.00126993563273167f, 0.002673415576968640f,
         0.004061331586355210f},{0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f,
                 0.00125871530333011f, 0.0014997750862781800f, 0.001823055274690330f,
                 0.001862907879059510f, 0.00126993563273167f, 0.002673415576968640f,
                 0.004061331586355210f}
    }
};

static RC_LookupTable g_c_dif_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {10829.999076136800f, 14390.588433361300f, 17705.78890692150f,
         43171.568400062000f, 26935.39645917410f, 32112.190637379200f,
         22994.811807973600f, 31368.700780914500f, 21714.92126684060f,
         12507.460992043400f},{10829.999076136800f, 14390.588433361300f, 17705.78890692150f,
                 43171.568400062000f, 26935.39645917410f, 32112.190637379200f,
                 22994.811807973600f, 31368.700780914500f, 21714.92126684060f,
                 12507.460992043400f}
    }
};

static RC_LookupTable g_r0_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f,
         0.005032740842521390f, 0.005065978524900830f, 0.00499394960654918f,
         0.005033886178286290f, 0.005035637273081450f, 0.005094409985592770f,
         0.0060653011513259000f},{0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f,
                 0.005032740842521390f, 0.005065978524900830f, 0.00499394960654918f,
                 0.005033886178286290f, 0.005035637273081450f, 0.005094409985592770f,
                 0.0060653011513259000f}
    }
};

/* ============================================================================
 * Minimal CSV parsing (header-driven column lookup, comma-separated, no quoting)
 * ========================================================================== */

#define MAX_LINE 1024
#define MAX_COLS 32

typedef struct {
    int time_ms_idx;
    int voltage_idx;
    int current_idx;
    int temperature_idx;  /* -1 if absent */
} ColumnMap;

static void trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static int split_line(char *line, char *cols[], int max_cols) {
    int n = 0;
    char *tok = strtok(line, ",");
    while (tok != NULL && n < max_cols) {
        cols[n++] = tok;
        tok = strtok(NULL, ",");
    }
    return n;
}

static ColumnMap parse_header(char *line) {
    ColumnMap map = { -1, -1, -1, -1 };
    char *cols[MAX_COLS];
    int n = split_line(line, cols, MAX_COLS);
    for (int i = 0; i < n; i++) {
        trim(cols[i]);
        for (char *p = cols[i]; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(cols[i], "time_ms") == 0 || strcmp(cols[i], "time") == 0) {
            map.time_ms_idx = i;
        } else if (strcmp(cols[i], "voltage") == 0) {
            map.voltage_idx = i;
        } else if (strcmp(cols[i], "current") == 0) {
            map.current_idx = i;
        } else if (strcmp(cols[i], "temperature") == 0 || strcmp(cols[i], "temp") == 0) {
            map.temperature_idx = i;
        }
    }
    return map;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.csv> <output.csv>\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "r");
    if (!fin) {
        fprintf(stderr, "ERROR: cannot open input CSV '%s'\n", argv[1]);
        return 1;
    }

    FILE *fout = fopen(argv[2], "w");
    if (!fout) {
        fprintf(stderr, "ERROR: cannot open output CSV '%s'\n", argv[2]);
        fclose(fin);
        return 1;
    }

    if (!BatteryModel_VerifyTable()) {
        fprintf(stderr, "ERROR: battery OCV lookup table failed self-check\n");
        fclose(fin);
        fclose(fout);
        return 1;
    }

    char header_line[MAX_LINE];
    if (!fgets(header_line, sizeof(header_line), fin)) {
        fprintf(stderr, "ERROR: input CSV is empty\n");
        fclose(fin);
        fclose(fout);
        return 1;
    }
    trim(header_line);
    ColumnMap cols = parse_header(header_line);
    if (cols.time_ms_idx < 0 || cols.voltage_idx < 0 || cols.current_idx < 0) {
        fprintf(stderr,
                "ERROR: input CSV header must contain time_ms, voltage, and current columns "
                "(temperature is optional)\n");
        fclose(fin);
        fclose(fout);
        return 1;
    }
    if (cols.temperature_idx < 0) {
        fprintf(stderr, "NOTE: no temperature column found -- defaulting to 25.0 C for every row\n");
    }

    fprintf(fout, "time_ms,input_voltage,input_current,input_temperature,"
                   "soc_percent,voltage_ocv,r0,capacity_ah,soh_percent,"
                   "v_ct,v_dif,uncertainty,valid,bypassed\n");

    KalmanSOC kf;
    bool initialized = false;
    long row_num = 0;
    float last_update_ms = 0.0f;
    char raw[MAX_LINE];

    while (fgets(raw, sizeof(raw), fin)) {
        char parse_buf[MAX_LINE];
        strncpy(parse_buf, raw, sizeof(parse_buf) - 1);
        parse_buf[sizeof(parse_buf) - 1] = '\0';
        trim(parse_buf);
        if (parse_buf[0] == '\0') continue;  /* skip blank lines */

        char *field_cols[MAX_COLS];
        int n = split_line(parse_buf, field_cols, MAX_COLS);
        if (n <= cols.time_ms_idx || n <= cols.voltage_idx || n <= cols.current_idx) {
            fprintf(stderr, "WARNING: row %ld has too few columns, skipping\n", row_num + 1);
            row_num++;
            continue;
        }

        float time_ms = strtof(field_cols[cols.time_ms_idx], NULL);
        float voltage = strtof(field_cols[cols.voltage_idx], NULL);
        float current = strtof(field_cols[cols.current_idx], NULL);
        float temperature = 25.0f;
        if (cols.temperature_idx >= 0 && n > cols.temperature_idx) {
            temperature = strtof(field_cols[cols.temperature_idx], NULL);
        }

        if (!initialized) {
            /* Mirrors adBms6830_soc_init(): seed SOC from the first voltage/temperature
             * reading, then wire up the same R/C lookup tables as the real firmware. */
            KalmanSOC_InitFromVoltage(&kf, voltage, temperature);
            KalmanSOC_SetRCLookupTables(&kf, &g_r_ct_lut, &g_c_ct_lut, &g_r_dif_lut, &g_c_dif_lut, &g_r0_lut);
            initialized = true;
            last_update_ms = time_ms;
        } else if (time_ms - last_update_ms < SIL_UPDATE_PERIOD_MS) {
            /* Between real update ticks -- the firmware wouldn't have called
             * adBms6830_soc_update() here either, so skip this sample entirely rather
             * than feeding the filter data it would never see on the real vehicle. */
            row_num++;
            continue;
        }
        last_update_ms = time_ms;

        /* Mirrors adBms6830_soc_update(): one measurement, one KalmanSOC_Update() call. */
        SOC_Measurement meas;
        meas.voltage = voltage;
        meas.current = current;
        meas.temperature = temperature;
        meas.timestamp_ms = (uint32_t)(time_ms + 0.5f);

        SOC_Estimate estimate;
        bool ok = KalmanSOC_Update(&kf, &meas, &estimate);

        if (ok) {
            fprintf(fout, "%.3f,%.6f,%.6f,%.3f,%.6f,%.6f,%.8f,%.6f,%.6f,%.8f,%.8f,%.8f,%d,%d\n",
                    time_ms, voltage, current, temperature,
                    estimate.soc_percent, estimate.voltage_ocv, estimate.r0,
                    estimate.capacity_ah, estimate.soh_percent,
                    estimate.v_ct, estimate.v_dif, estimate.uncertainty,
                    estimate.valid ? 1 : 0, estimate.bypassed ? 1 : 0);
        } else {
            fprintf(stderr, "WARNING: row %ld: KalmanSOC_Update() rejected the update "
                            "(singular innovation covariance) -- writing NaN\n", row_num + 1);
            fprintf(fout, "%.3f,%.6f,%.6f,%.3f,nan,nan,nan,nan,nan,nan,nan,nan,0,0\n",
                    time_ms, voltage, current, temperature);
        }

        row_num++;
    }

    fclose(fin);
    fclose(fout);

    if (!initialized) {
        fprintf(stderr, "ERROR: no data rows found in input CSV\n");
        return 1;
    }

    fprintf(stderr, "Processed %ld rows -> %s\n", row_num, argv[2]);
    return 0;
}
