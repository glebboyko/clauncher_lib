#!/bin/bash

init_dir="$(pwd)"
lib_dir="$1"
built_dir="$2"

mkdir "${lib_dir}/build"
cd "${lib_dir}/build" || exit 1

# setting cmake foo
cmake_foo="cmake"
if ! command -v cmake &> /dev/null
then
  cmake_foo="/opt/homebrew/bin/cmake"
fi

${cmake_foo} ..
${cmake_foo} --build ./

cd "${init_dir}" || exit 2

for (( i=3; i<=$#; i++ ))
do
  cp "${lib_dir}/build"/*"${!i}" "${built_dir}"
done