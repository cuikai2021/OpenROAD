#!/bin/bash

set -eu

cd "$(dirname $(readlink -f $0))/../"

_help() {
    cat <<EOF
usage: $0 [dynamic]
       $0 static

EOF
    exit "${1:-1}"
}

_lcov() {
    ./test/regression | tee /dev/null

    mkdir -p coverage_results
    lcov \
        --capture \
        --directory ./build \
        --exclude "/usr/include/*" \
        --exclude "/opt/*" \
        --exclude "/usr/lib/*" \
        --exclude "/usr/local/*" \
        --exclude "*build*" \
        --output-file ./coverage_results/main_coverage.info > ./coverage_results/coverage.log

    genhtml ./coverage_results/main_coverage.info \
        --output-directory ./coverage_results \
        --ignore-errors source > ./coverage_results/coverage.log

}

_coverity() {
    cmake -B build .
    cov-build --dir cov-int cmake --build build -j $(nproc)
    tar czvf openroad.tgz cov-int
    commitSha="$(git rev-parse HEAD)"
    curl --form token=$COVERITY_TOKEN \
         --form email=openroad@eng.ucsd.edu \
         --form file=@openroad.tgz \
         --form version="$commitSha" \
         "https://scan.coverity.com/builds?project=The-OpenROAD-Project%2FOpenROAD"
}

target="${1:-dynamic}"
case "${target}" in
    dynamic )
        _lcov
        ;;
    static )
        _coverity
        ;;
    *)
        echo "invalid argument: ${1}" >&2
        _help
        ;;
esac
