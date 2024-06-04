#!/bin/bash

# grep "request throughput" ./tmp | awk -F"throughput " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "Request throughput: %.4f\n", SUM}'

# grep "transaction throughput" ./tmp | awk -F"throughput " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "Transaction throughput: %.4f\n", SUM}'

# grep "abort count" ./tmp | awk -F"abort count " 'BEGIN {SUM = 0} {SUM += $2} END {printf "Abort count: %d\n", SUM}'

# grep "crt switch cnt" ./tmp | awk -F"crt switch cnt" 'BEGIN {SUM = 0} {SUM += $2} END {printf "Crt switch count: %d\n", SUM}'

# grep "tick" tmp | awk '{a[$7]+=$9} END {for (i in a) print "tick " i " total thpt " a[i]}' | sort -k2n

grep $1 ./tmp | awk -F"throughput " 'BEGIN {SUM = 0.0000} {SUM += $2} END {printf "%.2f\n", SUM}'
