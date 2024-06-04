#!/bin/bash

grep "Avg latency" ./tmp | awk -F" lock " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "Avg lock latency: %.4f us\n", SUM / 7}'
grep "Avg latency" ./tmp | awk -F" exec " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "Avg exec latency: %.4f us\n", SUM / 7}'
grep "Avg latency" ./tmp | awk -F" unlock " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "Avg unlock latency: %.4f us\n", SUM / 7}'