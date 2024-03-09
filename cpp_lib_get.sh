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

git_link="https://github.com/glebboyko"
req_libs=("c_tcp_lib")
lib_getter="cpp_lib_getter.sh"

dir_creator "${lib_dir}"

for lib in "${req_libs[@]}"
do
  (cd "${lib_dir}" || exit 1; git clone "${git_link}/${lib}")
  (cd "${lib_dir}/${lib}" || exit 1; /bin/bash "${lib_dir}/${lib}/${lib_getter}")
done
