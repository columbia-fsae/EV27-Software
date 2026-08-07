import sys
from pathlib import Path

import numpy as np
import pandas as pd
import argparse
ROOT = Path(__file__).resolve().parent


def generate(input_csv, cell_index: str = 'A1 (2)') -> pd.DataFrame:
    df = pd.read_csv(input_csv, skiprows=lambda x: x < 14 or x in [15, 16, 17])
    voltage_name = "BMS V " + cell_index
    temp_name = "BMS T " + cell_index
    return pd.DataFrame({
        "time_ms": df['Time'] * 1000.0,
        "voltage": df[voltage_name],
        "current": df['INV DC Bus Current'],
        "temperature": df[temp_name],
    })


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "input_csv", nargs="?", type=Path,
        default=ROOT / "example_data" / "synthetic_drive_cycle.csv",
        help="CSV with time_ms, voltage, current[, temperature] columns",
    )
    parser.add_argument(
        "--cell_id", type=str,
        default = "A5"
    )

    args = parser.parse_args()

    
    if not args.input_csv.exists():
        print(f"Input CSV not found: {args.input_csv}")
        if args.input_csv == ROOT / "example_data" / "synthetic_drive_cycle.csv":
            print("Generate it first with: python generate_example_data.py")
        sys.exit(1)
    input_csv = args.input_csv
    out_path = Path(__file__).resolve().parent / "example_data" / "example_1.csv"
    df = generate(input_csv, args.cell_id)
    df.to_csv(out_path, index=False)
    print(f"Wrote {len(df)} rows to {out_path}")
