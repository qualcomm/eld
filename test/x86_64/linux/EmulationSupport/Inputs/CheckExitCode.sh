#!/usr/bin/env bash

eval "$1"
return_code=$?
if [[ return_code -eq "$2" ]]; then
  exit 0
else
  exit 1
fi
