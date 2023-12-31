#!/bin/bash

ctcp_dir="$1"
built_dir="$2"

ctcp_cont=$(ls "${ctcp_dir}" | wc -l)

ctcp_link="https://github.com/glebboyko/c_tcp_lib.git"

if [[ ${ctcp_cont} -eq 0 ]]; then
  git clone "${ctcp_link}" "${ctcp_dir}"
else
  git -C "${ctcp_dir}" pull
fi

mkdir "${ctcp_dir}/cpp/build"

cd "${ctcp_dir}/cpp/build"

cmake ..
cmake --build ./

cp "${ctcp_dir}/cpp/build/"*.a "${built_dir}"