#!/bin/sh

#  fw_gen.sh
#  itlwm
#
#  Created by qcwap on 2020/3/10.
#  Copyright © 2020 钟先耀. All rights reserved.
set -eu
target_file="${PROJECT_DIR}/include/FwBinary.cpp"
while [ $# -gt 0 ];
do
    case $1 in
    -P) fw_files=$2
    shift
    ;;
    
    esac
    shift
done

script_file="${PROJECT_DIR}/scripts/zlib_compress_fw.py"
# Always inspect the inputs, including newly added firmware. Preserve the output
# timestamp if identical, so incremental builds do not retain a stale fwList.
temporary_file=$(mktemp "${target_file}.XXXXXX")
trap 'rm -f "$temporary_file"' EXIT HUP INT TERM
python3 "${script_file}" "${temporary_file}" "${fw_files}"
if ! cmp -s "${temporary_file}" "${target_file}"; then
    mv "${temporary_file}" "${target_file}"
fi
