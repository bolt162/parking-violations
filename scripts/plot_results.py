#!/usr/bin/env python3
"""
plot_results.py — Generate benchmark plots for Phase 1 (serial baseline).

Usage:
    python3 plot_results.py <results_dir>

Reads:
    results_dir/load_benchmark.csv
    results_dir/search_linear.csv
    results_dir/search_indexed.csv

Generates:
    results_dir/search_time_comparison.png
    results_dir/search_throughput_comparison.png
    results_dir/linear_vs_indexed.png
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import numpy as np


def load_csv(filepath):
    """Load a benchmark CSV file."""
    if not os.path.exists(filepath):
        print(f"  Warning: {filepath} not found, skipping")
        return None
    df = pd.read_csv(filepath)
    return df


def plot_search_times(linear_df, indexed_df, output_dir):
    """Bar chart: mean query time (ms) per workload, linear vs indexed."""
    fig, ax = plt.subplots(figsize=(14, 7))

    names = linear_df['name'].values
    x = np.arange(len(names))
    width = 0.35

    linear_times = linear_df['mean_ms'].values
    linear_std = linear_df['stddev_ms'].values

    bars1 = ax.bar(x - width/2, linear_times, width,
                   yerr=linear_std, capsize=3,
                   label='LinearSearch', color='#4C72B0', alpha=0.85)

    if indexed_df is not None:
        indexed_times = indexed_df['mean_ms'].values
        indexed_std = indexed_df['stddev_ms'].values
        bars2 = ax.bar(x + width/2, indexed_times, width,
                       yerr=indexed_std, capsize=3,
                       label='IndexedSearch', color='#DD8452', alpha=0.85)

    ax.set_xlabel('Workload', fontsize=12)
    ax.set_ylabel('Mean Time (ms)', fontsize=12)
    ax.set_title('Phase 1: Search Query Times (Serial Baseline)', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=11)
    ax.grid(axis='y', alpha=0.3)

    # Add value labels on bars
    for bar in bars1:
        height = bar.get_height()
        if height > 0:
            ax.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.1f}',
                    ha='center', va='bottom', fontsize=7)

    if indexed_df is not None:
        for bar in bars2:
            height = bar.get_height()
            if height > 0:
                ax.text(bar.get_x() + bar.get_width()/2., height,
                        f'{height:.1f}',
                        ha='center', va='bottom', fontsize=7)

    plt.tight_layout()
    path = os.path.join(output_dir, 'search_time_comparison.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def plot_throughput(linear_df, indexed_df, output_dir):
    """Bar chart: throughput (million records/sec) per workload."""
    fig, ax = plt.subplots(figsize=(14, 7))

    names = linear_df['name'].values
    x = np.arange(len(names))
    width = 0.35

    linear_tp = linear_df['throughput_rec_per_sec'].values / 1e6
    bars1 = ax.bar(x - width/2, linear_tp, width,
                   label='LinearSearch', color='#4C72B0', alpha=0.85)

    if indexed_df is not None:
        indexed_tp = indexed_df['throughput_rec_per_sec'].values / 1e6
        bars2 = ax.bar(x + width/2, indexed_tp, width,
                       label='IndexedSearch', color='#DD8452', alpha=0.85)

    ax.set_xlabel('Workload', fontsize=12)
    ax.set_ylabel('Throughput (M records/sec)', fontsize=12)
    ax.set_title('Phase 1: Search Throughput (Serial Baseline)', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=45, ha='right', fontsize=9)
    ax.legend(fontsize=11)
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    path = os.path.join(output_dir, 'search_throughput_comparison.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def plot_speedup(linear_df, indexed_df, output_dir):
    """Bar chart: speedup (linear_time / indexed_time) per workload."""
    if indexed_df is None:
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    names = linear_df['name'].values
    x = np.arange(len(names))

    speedup = linear_df['mean_ms'].values / indexed_df['mean_ms'].values

    colors = ['#55A868' if s > 1.0 else '#C44E52' for s in speedup]
    bars = ax.bar(x, speedup, color=colors, alpha=0.85)

    ax.axhline(y=1.0, color='black', linestyle='--', linewidth=0.8, alpha=0.5)
    ax.set_xlabel('Workload', fontsize=12)
    ax.set_ylabel('Speedup (Linear / Indexed)', fontsize=12)
    ax.set_title('Phase 1: IndexedSearch Speedup over LinearSearch',
                 fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=45, ha='right', fontsize=9)
    ax.grid(axis='y', alpha=0.3)

    # Value labels
    for bar, s in zip(bars, speedup):
        ax.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                f'{s:.1f}x',
                ha='center', va='bottom', fontsize=9, fontweight='bold')

    plt.tight_layout()
    path = os.path.join(output_dir, 'linear_vs_indexed.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def plot_hit_rate(linear_df, output_dir):
    """Bar chart: hit rate (matched / scanned) per workload."""
    fig, ax = plt.subplots(figsize=(12, 6))

    names = linear_df['name'].values
    x = np.arange(len(names))

    hit_rate = (linear_df['records_matched'].values /
                linear_df['records_scanned'].values * 100)

    bars = ax.bar(x, hit_rate, color='#8172B2', alpha=0.85)

    ax.set_xlabel('Workload', fontsize=12)
    ax.set_ylabel('Hit Rate (%)', fontsize=12)
    ax.set_title('Phase 1: Query Selectivity (% Records Matched)', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=45, ha='right', fontsize=9)
    ax.grid(axis='y', alpha=0.3)

    for bar, hr in zip(bars, hit_rate):
        ax.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                f'{hr:.1f}%',
                ha='center', va='bottom', fontsize=8)

    plt.tight_layout()
    path = os.path.join(output_dir, 'query_selectivity.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_results.py <results_dir>")
        sys.exit(1)

    results_dir = sys.argv[1]

    print(f"Loading results from: {results_dir}")
    print()

    # Load CSVs
    load_df = load_csv(os.path.join(results_dir, 'load_benchmark.csv'))
    linear_df = load_csv(os.path.join(results_dir, 'search_linear.csv'))
    indexed_df = load_csv(os.path.join(results_dir, 'search_indexed.csv'))

    if linear_df is None:
        print("ERROR: search_linear.csv is required")
        sys.exit(1)

    print("Generating plots...")

    # Load benchmark summary
    if load_df is not None:
        print(f"\n  Load benchmark:")
        for _, row in load_df.iterrows():
            print(f"    {row['name']}: mean={row['mean_ms']:.1f} ms, "
                  f"throughput={row['throughput_rec_per_sec']:.0f} rec/s")

    # Search results summary table
    print(f"\n  Linear search results:")
    for _, row in linear_df.iterrows():
        print(f"    {row['name']:20s}: mean={row['mean_ms']:10.2f} ms, "
              f"matched={int(row['records_matched']):>10,}, "
              f"throughput={row['throughput_rec_per_sec']:>14,.0f} rec/s")

    if indexed_df is not None:
        print(f"\n  Indexed search results:")
        for _, row in indexed_df.iterrows():
            print(f"    {row['name']:20s}: mean={row['mean_ms']:10.2f} ms, "
                  f"matched={int(row['records_matched']):>10,}, "
                  f"throughput={row['throughput_rec_per_sec']:>14,.0f} rec/s")

    # Generate plots
    print()
    plot_search_times(linear_df, indexed_df, results_dir)
    plot_throughput(linear_df, indexed_df, results_dir)
    plot_speedup(linear_df, indexed_df, results_dir)
    plot_hit_rate(linear_df, results_dir)

    print(f"\nAll plots saved to: {results_dir}/")


if __name__ == '__main__':
    main()
