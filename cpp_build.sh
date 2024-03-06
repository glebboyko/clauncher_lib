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

if [[ "$#" -ne "1" ]]; then
  echo "USAGE:"
  echo ".sh %directory_to_save_built%"
  exit 2
fi

final_dir="$1"

build_dir="build"

dir_creator "${build_dir}"

(cd "${build_dir}" || exit 1; cmake ..; cmake --build ./)

cp "${build_dir}/"*.a ${build_dir}/clauncher_* "${final_dir}"

rm -rf "${build_dir}"

exit 0
