#!/bin/bash

set -eE

trap "_exit" ERR

function _exit {
  echo -e "\nThere seems to be some problems, probably a network problem, if you want to continue fetch just rerun $0"
}

function _expr {
  expr "$@" || :
}

function progress_bar {
  progress=$(_expr $1 \* 100 / $2 \* 100 / 100 )
  done=$(_expr $progress \* 4 / 10)
  left=$(_expr 40 - $done)
	done=$(printf "%${done}s")
	left=$(printf "%${left}s")
  tput rc
  echo -ne "Progress : [${done// /#}${left// /-}] ${progress}%\n"
  if [ -n "$3" ]; then
    tput el
    echo -ne "Fetching files for $3\n"
  else
    tput el
    echo -ne "Done!\n"
  fi
}

function setup_screen {
  for i in $(seq 1 $1); do
    echo -ne "\n"
  done
  for i in $(seq 1 $1); do
    tput cuu1
  done
  tput sc
}

function fetch {
  setup_screen 2
  toml_files=($(find $1 -name "*.toml"))
  toml_count=${#toml_files[@]}
  fetch_count=0
  for toml in ${toml_files[@]}; do
    progress_bar $fetch_count $toml_count "$toml"
    icb fetch -w $(dirname $toml) $toml > /dev/null
    fetch_count=$(expr $fetch_count + 1)
  done
  progress_bar $fetch_count $toml_count ""
}

WANT_TO_FETCH=()
while [ "$#" -gt 0 ]; do
  case "${1}" in
    -f | --fetch)
      WANT_TO_FETCH+=($2)
      shift 1
      ;;
    -h | --help)
      echo -ne "usage: ./fetch.sh [-f/--fetch {DIR}] \
              \nexample: \
              \n  ./etc/fetch.sh    # fetch all \
              \n  ./etc/fetch.sh -f src/linden -f src/Banyan     # only fetch 'src/linden' and 'src/Banyan'\
              \n"
      exit 0
      ;;
  esac
  shift 1
done
if [ "${#WANT_TO_FETCH[@]}" == "0" ]; then
  fetch .
else
  for dir in ${WANT_TO_FETCH[@]}; do
    echo fetching  $dir
    fetch $dir
  done
fi
