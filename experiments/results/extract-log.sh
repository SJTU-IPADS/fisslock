#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

benchmark=${1:?"Usage: extract-log.sh [benchmark] [log_type]"}
log_type=${2:?"Usage: extract-log.sh [benchmark] [log_type]"}

for file in $RESULT_PATH/$benchmark/lat*raw; do
  filename=$(basename -- "$file")
  new_file_name=$RESULT_PATH/$benchmark/${filename}-${log_type}
  if [[ ! -f "$new_file_name" || "$file" -nt "${new_file_name}" ]]; then
    grep ",$log_type$" "$file" > "${new_file_name}"
    echo "extracted $log_type logs from $file to $new_file_name"
  fi
done