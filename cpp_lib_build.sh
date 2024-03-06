#!/bin/bash

#args: directory to save built

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

init_dir="$(pwd)"

lib_dir="libs"
lib_built_dir="${lib_dir}/built"

ext_lib_builder="cpp_lib_builder.sh"
lib_builder="cpp_build.sh"

req_libs=("c_tcp_lib")

dir_creator "${lib_built_dir}"

for lib in "${req_libs[@]}"
do
  (cd "${lib_dir}/${lib}" || exit 1 ; /bin/bash ${ext_lib_builder}; /bin/bash "${lib_builder}" "${init_dir}/${lib_built_dir}")
done

exit 0