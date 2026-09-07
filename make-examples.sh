#!/bin/bash

set -ue

lst=$(find ./examples -mindepth 1 -type d | awk -F '/' '{ print $3 }')

cd build || exit 1

for item in ${lst[@]}; do
  make ${item}
done


