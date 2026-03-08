#!/usr/bin/env python3
"""
preprocess.py — Merge and normalize three NYC Parking Violations CSV files
(FY2024, FY2025, FY2026) into a single unified CSV.

Normalizations applied:
  1. Unified 44-column schema (adds Fiscal Year to FY2024/2025)
  2. FY2026 Issue Date empty → fallback to Date First Observed
  3. FY2026 Date First Observed: strip commas ("20,250,615" → "20250615")
  4. Null unification: "", "0", "00000000", "88888888" → "" (empty)
  5. Violation County: multiple codes per borough → canonical 2-letter code
  6. Vehicle Color: variant spellings → canonical short code
  7. Header whitespace trimming (FY2026 has trailing spaces)

Usage:
  python3 preprocess.py <input_dir> <output_file>
  python3 preprocess.py ../Parking_Violations_Data parking_violations_merged.csv
"""

import csv
import os
import sys
import time
import re

# ─── Canonical column order (44 columns) ────────────────────────────────────

CANONICAL_HEADERS = [
    "Summons Number",
    "Plate ID",
    "Registration State",
    "Plate Type",
    "Issue Date",
    "Violation Code",
    "Vehicle Body Type",
    "Vehicle Make",
    "Issuing Agency",
    "Street Code1",
    "Street Code2",
    "Street Code3",
    "Vehicle Expiration Date",
    "Violation Location",
    "Violation Precinct",
    "Issuer Precinct",
    "Issuer Code",
    "Issuer Command",
    "Issuer Squad",
    "Violation Time",
    "Time First Observed",
    "Violation County",
    "Violation In Front Of Or Opposite",
    "House Number",
    "Street Name",
    "Intersecting Street",
    "Date First Observed",
    "Law Section",
    "Sub Division",
    "Violation Legal Code",
    "Days Parking In Effect",
    "From Hours In Effect",
    "To Hours In Effect",
    "Vehicle Color",
    "Unregistered Vehicle?",
    "Vehicle Year",
    "Meter Number",
    "Feet From Curb",
    "Violation Post Code",
    "Violation Description",
    "No Standing or Stopping Violation",
    "Hydrant Violation",
    "Double Parking Violation",
    "Fiscal Year",
]

# Column indices (0-based) in the canonical order
IDX_ISSUE_DATE = 4
IDX_VIOLATION_COUNTY = 21
IDX_DATE_FIRST_OBSERVED = 26
IDX_VEHICLE_COLOR = 33
IDX_VEHICLE_EXP_DATE = 12
IDX_FISCAL_YEAR = 43

# ─── Normalization maps ─────────────────────────────────────────────────────

COUNTY_MAP = {
    # Manhattan
    "NY":    "MN",
    "MN":    "MN",
    # Brooklyn / Kings
    "BK":    "BK",
    "K":     "BK",
    "KINGS": "BK",
    # Queens
    "QN":    "QN",
    "Q":     "QN",
    "QNS":   "QN",
    # Bronx
    "BX":    "BX",
    "BRONX": "BX",
    # Staten Island / Richmond
    "ST":    "SI",
    "R":     "SI",
    "RICH":  "SI",
    "RC":    "SI",
}

COLOR_MAP = {
    # Black
    "B":     "BK",
    "BK":    "BK",
    "BLK":   "BK",
    "BLCK":  "BK",
    "BLACK": "BK",
    # White
    "W":     "WH",
    "WH":    "WH",
    "WHI":   "WH",
    "WHT":   "WH",
    "WHTE":  "WH",
    "WHITE": "WH",
    "WT":    "WH",
    # Gray
    "GY":    "GY",
    "GRY":   "GY",
    "GRAY":  "GY",
    "GREY":  "GY",
    # Blue
    "BL":    "BL",
    "BLE":   "BL",
    "BLU":   "BL",
    "BLUE":  "BL",
    # Red
    "RD":    "RD",
    "RED":   "RD",
    # Green
    "G":     "GN",
    "GN":    "GN",
    "GR":    "GN",
    "GRN":   "GN",
    "GREEN": "GN",
    # Silver
    "SL":    "SL",
    "SIL":   "SL",
    "SILVE": "SL",
    "SILVR": "SL",
    # Brown
    "BR":    "BR",
    "BROWN": "BR",
    # Tan
    "TN":    "TN",
    "TAN":   "TN",
    # Gold
    "GL":    "GL",
    "GLD":   "GL",
    "GOLD":  "GL",
    # Maroon
    "MR":    "MR",
    "MARON": "MR",
    # Orange
    "OR":    "OR",
    "ORANG": "OR",
    # Yellow
    "YW":    "YW",
    "YELLO": "YW",
    # Purple
    "PR":    "PR",
    "PURPL": "PR",
    # Pink
    "PK":    "PK",
    "PNK":   "PK",
    "PINK":  "PK",
    # Beige
    "BG":    "BG",
    "BEIGE": "BG",
    # Cream / Ivory
    "CR":    "CR",
    "CREAM": "CR",
    "IVORY": "CR",
    # Copper
    "CP":    "CP",
    # Light / Dark variants
    "LTBL":  "LB",
    "DKBL":  "DB",
    "LTGY":  "LG",
    "LTG":   "LG",
    "DKGY":  "DG",
    "DKGR":  "DG",
}

NULL_DATE_VALUES = {"", "0", "00000000", "88888888"}


def detect_fiscal_year(filename: str) -> str:
    """Extract fiscal year from filename like '..._Fiscal_Year_2025_...'."""
    match = re.search(r"Fiscal_Year_(\d{4})", filename)
    if match:
        return match.group(1)
    raise ValueError(f"Cannot detect fiscal year from filename: {filename}")


def strip_commas(value: str) -> str:
    """Remove thousand-separator commas from a numeric string."""
    return value.replace(",", "")


def normalize_county(raw: str) -> str:
    """Normalize violation county to canonical 2-letter code."""
    key = raw.strip().upper()
    return COUNTY_MAP.get(key, key)


def normalize_color(raw: str) -> str:
    """Normalize vehicle color to canonical short code."""
    key = raw.strip().upper()
    return COLOR_MAP.get(key, key)


def normalize_null_date(value: str) -> str:
    """Normalize null date representations to empty string."""
    stripped = value.strip()
    if stripped in NULL_DATE_VALUES:
        return ""
    return stripped


def process_file(filepath: str, fiscal_year: str, writer: csv.writer) -> int:
    """Process a single CSV file and write normalized rows to the writer.
    Returns the number of data rows written."""
    count = 0
    is_fy2026 = (fiscal_year == "2026")

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        reader = csv.reader(f)

        # Read and normalize header
        raw_header = next(reader)
        header = [h.strip() for h in raw_header]
        num_cols = len(header)

        # Build column index mapping: canonical name → position in this file
        col_map = {}
        for i, h in enumerate(header):
            col_map[h] = i

        for row in reader:
            # Pad short rows with empty strings
            while len(row) < num_cols:
                row.append("")

            # Build output row in canonical order
            out = [""] * 44

            for ci, canon_name in enumerate(CANONICAL_HEADERS):
                if canon_name == "Fiscal Year":
                    # Handle separately below
                    continue
                src_idx = col_map.get(canon_name)
                if src_idx is not None and src_idx < len(row):
                    out[ci] = row[src_idx].strip()
                else:
                    out[ci] = ""

            # ── Normalizations ──

            # Date First Observed: strip commas (FY2026 issue)
            raw_dfo = out[IDX_DATE_FIRST_OBSERVED]
            if "," in raw_dfo:
                raw_dfo = strip_commas(raw_dfo)
            out[IDX_DATE_FIRST_OBSERVED] = normalize_null_date(raw_dfo)

            # Issue Date: if empty, fallback to Date First Observed
            raw_issue = out[IDX_ISSUE_DATE]
            raw_issue = normalize_null_date(raw_issue)
            if not raw_issue and out[IDX_DATE_FIRST_OBSERVED]:
                raw_issue = out[IDX_DATE_FIRST_OBSERVED]
            out[IDX_ISSUE_DATE] = raw_issue

            # Vehicle Expiration Date: normalize nulls
            out[IDX_VEHICLE_EXP_DATE] = normalize_null_date(out[IDX_VEHICLE_EXP_DATE])

            # Violation County
            if out[IDX_VIOLATION_COUNTY]:
                out[IDX_VIOLATION_COUNTY] = normalize_county(out[IDX_VIOLATION_COUNTY])

            # Vehicle Color
            if out[IDX_VEHICLE_COLOR]:
                out[IDX_VEHICLE_COLOR] = normalize_color(out[IDX_VEHICLE_COLOR])

            # Fiscal Year
            if is_fy2026:
                fy_idx = col_map.get("Fiscal Year")
                if fy_idx is not None and fy_idx < len(row):
                    out[IDX_FISCAL_YEAR] = row[fy_idx].strip() or fiscal_year
                else:
                    out[IDX_FISCAL_YEAR] = fiscal_year
            else:
                out[IDX_FISCAL_YEAR] = fiscal_year

            writer.writerow(out)
            count += 1

            if count % 2_000_000 == 0:
                print(f"  ... {count:,} rows processed from FY{fiscal_year}")

    return count


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_dir> <output_file>")
        print(f"Example: {sys.argv[0]} ../Parking_Violations_Data parking_violations_merged.csv")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_file = sys.argv[2]

    # Find CSV files and sort by fiscal year
    csv_files = []
    for fname in sorted(os.listdir(input_dir)):
        if fname.endswith(".csv") and "Parking_Violations" in fname:
            filepath = os.path.join(input_dir, fname)
            try:
                fy = detect_fiscal_year(fname)
                csv_files.append((filepath, fy))
            except ValueError as e:
                print(f"  Skipping {fname}: {e}")

    if not csv_files:
        print(f"ERROR: No parking violation CSVs found in {input_dir}")
        sys.exit(1)

    csv_files.sort(key=lambda x: x[1])  # Sort by fiscal year

    print(f"Found {len(csv_files)} CSV file(s):")
    for fp, fy in csv_files:
        size_gb = os.path.getsize(fp) / (1024 ** 3)
        print(f"  FY{fy}: {os.path.basename(fp)} ({size_gb:.2f} GB)")

    print(f"\nOutput: {output_file}")
    print(f"Canonical columns: {len(CANONICAL_HEADERS)}")
    print()

    total_rows = 0
    per_fy_counts = {}
    t_start = time.time()

    with open(output_file, "w", newline="", encoding="utf-8") as outf:
        writer = csv.writer(outf, quoting=csv.QUOTE_ALL)

        # Write header
        writer.writerow(CANONICAL_HEADERS)

        # Process each file
        for filepath, fy in csv_files:
            print(f"Processing FY{fy}: {os.path.basename(filepath)}")
            t_file_start = time.time()
            count = process_file(filepath, fy, writer)
            t_file_end = time.time()

            per_fy_counts[fy] = count
            total_rows += count
            elapsed = t_file_end - t_file_start
            rate = count / elapsed if elapsed > 0 else 0
            print(f"  Done: {count:,} rows in {elapsed:.1f}s ({rate:,.0f} rows/sec)\n")

    t_end = time.time()
    total_elapsed = t_end - t_start

    # Print summary
    output_size_gb = os.path.getsize(output_file) / (1024 ** 3)
    print("=" * 60)
    print("PREPROCESSING COMPLETE")
    print("=" * 60)
    print(f"Total rows written:  {total_rows:,}")
    for fy, cnt in sorted(per_fy_counts.items()):
        print(f"  FY{fy}: {cnt:,}")
    print(f"Output file:         {output_file}")
    print(f"Output size:         {output_size_gb:.2f} GB")
    print(f"Total time:          {total_elapsed:.1f}s")
    print(f"Overall throughput:  {total_rows / total_elapsed:,.0f} rows/sec")


if __name__ == "__main__":
    main()
