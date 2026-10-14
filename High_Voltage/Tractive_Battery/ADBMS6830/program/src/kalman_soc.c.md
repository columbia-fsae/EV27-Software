# kalman_soc.c.md

## 1. crc32_table

Purpose: A precomputed table of constants used to calculate CRC32 checksums efficiently.

Takes in: Nothing, it is a constant table.

Returns: Nothing. The table provides values used by the CRC32 calculation to detect accidental corruption in saved battery estimation data.

## 2. calculate_crc32(const uint8_t* data, uint32_t length)

Purpose: Calculates a CRC32 checksum for a block of data.

Takes in:

- data: Pointer to the bytes to check.
- length: Number of bytes to process.

Returns: A uint32_t checksum. A 32 bit unsigned integer that can be used to detect whether saved data has been accidentally corrupted or changed. 

## 3. interp2d(const RC_LookupTable* lut, float soc, float temp)

Purpose: Performs bilinear interpolation on a two dimensional lookup table, clamping inputs to the table's boundaries.

Takes in:

- lut: The lookup table containing values indexed by SOC and temperature.
- soc: Battery state of charge.
- temp: Battery temperature.

Returns: An interpolated float, an estimated table value for the specified SOC and temperature, even when those inputs fall between the table's stored points. For example, it can estimate a battery resistance at 45% SOC and 25°C without requiring an exact entry for that combination.

## 4. interp1d(const float* x, const float* y, uint16_t size, float xi)

Purpose: Performs linear interpolation between points in a one dimensional lookup table.

Takes in:

- x: Array of known input coordinates.
- y: Array of corresponding known values.
- size: Number of entries in the arrays.
- xi: Input coordinate at which to estimate a value.

Returns: An interpolated float, estimated value corresponding to xi. Example, it can estimate battery capacity between two measured SOC points rather than using only the nearest stored value.

## 5. KalmanSOC_Init(KalmanSOC* kf, float initial_soc)

Purpose: Initializes the Kalman filter's internal states, parameters, covariance matrices, and tracking variables.

Takes in:

- kf: Pointer to the filter structure to initialize.
- initial_soc: Starting estimate of the battery's state of charge.

Returns: Nothing (void). It prepares the filter to begin estimating battery SOC and related parameters, using the supplied SOC as its initial estimate.

## 6. KalmanSOC_InitFromVoltage(KalmanSOC* kf, float rest_voltage, float temp_c)

Purpose: Initializes the filter using an estimate of SOC derived from the battery's open circuit voltage.

Takes in:

- kf: Pointer to the filter structure.
- rest_voltage: Battery voltage measured when the battery is sufficiently at rest.
- temp_c: Temperature in degrees Celsius.

Returns: Nothing (void). It uses BatteryModel_GetSocFromOcv to estimate the starting SOC, then initializes the filter with that value. In the provided implementation, temp_c is not used.

## 7. KalmanSOC_InitFromFlash(KalmanSOC* kf, const KalmanSOC_PersistentState* persistent)

Purpose: Restores the filter from previously saved persistent data.

Takes in:

- kf: Pointer to the filter to restore.
- persistent: Pointer to the saved state, including previous estimates and parameters.

Returns: A bool. True if the saved state is valid and successfully restored, false if validation fails. This lets the calling code decide whether to resume the previous estimation state or initialize the filter from scratch.

## 8. KalmanSOC_SetRCLookupTables(...)

Purpose: Assigns the lookup tables used by the battery model.

Takes in:

- kf: Pointer to the filter.
- r_ct: Charge-transfer resistance lookup table.
- c_ct: Charge-transfer capacitance lookup table.
- r_dif: Diffusion resistance lookup table.
- c_dif: Diffusion capacitance lookup table.
- r_0: Ohmic resistance lookup table.

Returns: Nothing (void). It stores references to the tables so the estimator can obtain battery electrical parameters for different operating conditions, rather than relying on fixed values.

## 9. KalmanSOC_GetPersistentState(const KalmanSOC* kf, KalmanSOC_PersistentState* persistent)

Purpose: Packages the current filter state into a structure that can be saved to persistent storage, such as flash memory.

Takes in:

- kf: Pointer to the current filter.
- persistent: Pointer to the structure that will receive the saved data.

Returns: Nothing (void). It copies the relevant estimates and parameters, adds a magic number identifying the data format, and calculates a CRC so the saved state can later be checked for corruption.

## 10. KalmanSOC_CalculateCRC32(const KalmanSOC_PersistentState* persistent)

Purpose: Calculates the CRC32 checksum for a persistent-state structure.

Takes in:

- persistent: Pointer to the saved state.

Returns: A uint32_t checksum, a 32 bit unsigned integer calculated from the saved structure, excluding its CRC field. The checksum is stored with the data and recalculated later to check whether the saved contents have changed unexpectedly.

## 11. KalmanSOC_ValidatePersistentState(const KalmanSOC_PersistentState* persistent)

Purpose: Checks whether saved filter data is structurally valid and plausible.

Takes in:

- persistent: Pointer to the saved state.

Returns: A bool, true if the magic number, checksum, and configured sanity checks pass; otherwise false. This prevents the estimator from blindly restoring data that appears corrupted or contains implausible values for SOC, resistance, capacity, or state of health.

## 12. KalmanSOC_Update(KalmanSOC* kf, const SOC_Measurement* meas, SOC_Estimate* estimate)

Purpose: Performs the main Kalman filter update, using new measurements to predict and correct the estimated battery state.

Takes in:

- kf: Pointer to the filter and its current internal state.
- meas: Pointer to the latest measurements, such as voltage, current, temperature, and timestamp.
- estimate: Pointer to the output structure to populate.

Returns: A bool, true when an estimate is produced, including the prediction-only path if the implementation permits it; false if the update fails its determinant check.

When successful, the output structure contains the calculated battery estimates, such as:

- SOC: Estimated remaining charge as a proportion of usable capacity.
- SOH: Estimated state of health, indicating battery condition relative to its reference condition.
- Resistance and capacity estimates: Parameters that help the model account for changes in battery behaviour.
- Other state estimates: Values used by the battery model to represent its internal electrical behaviour.

The output structure is pointed to by the estimate argument that was passed in.

## 13. KalmanSOC_Reset(KalmanSOC* kf)

Purpose: Resets the filter by reinitializing it using its current SOC estimate.

Takes in:

- kf: Pointer to the filter.

Returns: Nothing (void). It resets the filter's internal estimation state and covariance settings while using the current SOC as the new starting point, rather than necessarily returning SOC to zero.

## 14. KalmanSOC_GetEstimate(const KalmanSOC* kf, SOC_Estimate* estimate)

Purpose: Retrieves the filter's current estimate without performing another Kalman update.

Takes in:

- kf: Pointer to the filter.
- estimate: Pointer to the output structure.

Returns: Nothing (void). It copies the current estimated battery values into estimate, allowing other parts of the software to read the latest result without changing the filter's state.

## 15. KalmanSOC_IsHealthy(const KalmanSOC* kf)

Purpose: Checks whether the filter has been initialized.

Takes in:

- kf: Pointer to the filter.

Returns: A bool, true if the filter's initialized flag is set, otherwise false. This indicates initialization status, not necessarily that every estimate is accurate or that the battery itself is healthy.

## 16. KalmanSOC_GetStats(const KalmanSOC* kf, float* max_time_us, float* avg_time_us)

Purpose: Provides the filter's execution-time statistics.

Takes in:

- kf: Pointer to the filter.
- max_time_us: Output pointer for maximum execution time, in microseconds.
- avg_time_us: Output pointer for average execution time, in microseconds.

Returns: Nothing (void). Both output values are set to zero.
