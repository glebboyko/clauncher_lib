#!/bin/bash

libs_path="$1"

# getting git libs
git_libs=(c_tcp_lib)
git_link="https://git.lafresa.ru/glebboiko"

for lib in ${git_libs[*]}
do
  rm -rf "${libs_path}/${lib}"
  git clone "${git_link}/${lib}.git" "${libs_path}/${lib}"
done
