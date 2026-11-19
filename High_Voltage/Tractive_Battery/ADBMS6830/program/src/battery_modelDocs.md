# battery_model.cpp.md

## 1. g_ocv_lut

Purpose: Constant lookup table mapping state of charge (SOC) to open circuit voltage (OCV) and curve slope for a typical Li-ion cell.

Takes in: Nothing, it is a constant table.

Returns: Nothing. Provides the SOC, OCV, and slope points used by the other functions.

## 2. binary_search_lut(float value, bool is_soc)

Purpose: Finds which table segment a value falls into so it can be interpolated.

Takes in:

- value: SOC or OCV value to search for.
- is_soc: true to search by SOC, false to search by OCV.

Returns: An int, the index of the lower point of the segment containing value.

## 3. interpolate(float x, float x0, float x1, float y0, float y1)

Purpose: Performs linear interpolation between two known points.

Takes in:

- x: Point at which to estimate a value.
- x0, x1, y0, y1: The two known points.

Returns: A float, the estimated value at x. Returns y0 if x0 and x1 are equal to avoid division by zero.

## 4. BatteryModel_Init(void)

Purpose: Initializes the battery model; currently an empty placeholder.

Takes in: Nothing.

Returns: Nothing (void).

## 5. BatteryModel_GetOcv(float soc)

Purpose: Estimates open circuit voltage for a given SOC.

Takes in:

- soc: State of charge, from 0.0 to 1.0.

Returns: A float, the interpolated OCV in volts, clamped to the table's range.

## 6. BatteryModel_GetSocFromOcv(float ocv)

Purpose: Estimates SOC from open circuit voltage, the inverse of BatteryModel_GetOcv.

Takes in:

- ocv: Resting battery voltage in volts.

Returns: A float, the interpolated SOC. Note that voltages above the table's maximum can return an SOC slightly above 1.0.

## 7. BatteryModel_GetSlope(float soc)

Purpose: Estimates how quickly voltage changes with SOC at a given point on the OCV curve.

Takes in:

- soc: State of charge, from 0.0 to 1.0.

Returns: A float, the interpolated slope in V/SOC.

## 8. BatteryModel_GetResistance(float r_base, float temp_c)

Purpose: Returns internal resistance, intended to eventually be temperature compensated.

Takes in:

- r_base: Base resistance in ohms.
- temp_c: Temperature in degrees Celsius (currently unused).

Returns: A float, currently r_base unchanged.

## 9. BatteryModel_GetNearestIndex(float soc)

Purpose: Returns the table index for a given SOC, for debugging.

Takes in:

- soc: State of charge.

Returns: An int, the lower bound index of the surrounding segment (not necessarily the nearest point).

## 10. BatteryModel_GetTablePoint(int index, float* soc, float* ocv, float* slope)

Purpose: Reads a single table entry, for debugging or calibration.

Takes in:

- index: Position of the entry to read.
- soc, ocv, slope: Output pointers for the entry's values (any may be NULL).

Returns: A bool, true if index is valid and values were written, otherwise false.

## 11. BatteryModel_VerifyTable(void)

Purpose: Checks that the lookup table is consistent and not corrupted.

Takes in: Nothing.

Returns: A bool, true if SOC is strictly increasing, OCV has no large drops, and all voltages are between 2.0 V and 5.0 V; otherwise false.