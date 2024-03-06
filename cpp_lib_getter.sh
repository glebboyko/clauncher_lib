#!/bin/bash

function dir_creator {
  dir="$1"
  rm -rf "${dir}"
  mkdir "${dir}"
  test -d "${dir}"
  if [[ $(echo $?) -eq 1 ]]; then
    echo "CANNOT CREATE DIR"
    exit 1
  fi
}

lib_dir="libs"

git_link="https://git.lafresa.ru/glebboiko/"
req_libs=("c_tcp_lib")
lib_getter="cpp_lib_getter.sh"

final_dir="${lib_dir}/built"

dir_creator "${final_dir}"

for lib in "${req_libs[@]}"
do
  rm -rf "${lib_dir}/${lib}"
  git clone "${git_link}/${lib}" "${lib_dir}/${lib}"
  /bin/bash "${lib_dir}/${lib}/${lib_getter}"
done