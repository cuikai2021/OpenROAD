#!/bin/bash

set -euo pipefail

SUDO="";
PROXYCHAINS=""

_installGtest(){
    # gtest (latest)
    cd "${baseDir}"
    $PROXYCHAINS git clone https://github.com/google/googletest.git
    cd googletest
    cmake -B build .
    cmake --build build -j $(nproc) --target install
}

_installProtobuf(){
    cd "${baseDir}"
    $PROXYCHAINS git clone https://github.com/protocolbuffers/protobuf.git
    cd protobuf
    $PROXYCHAINS git submodule update --init --recursive
    cmake .
    cmake --build . --parallel 10
    ctest --verbose
    cmake --install .
}

_installCommonDev() {
    lastDir="$(pwd)"
    # tools versions
    # cmakeVersionBig=3.14
    cmakeVersionBig=3.18
    cmakeVersionSmall=${cmakeVersionBig}.0
    swigVersion=4.0.1
    boostVersionBig=1.76
    boostVersionSmall=${boostVersionBig}.0
    eigenVersion=3.3
    lemonVersion=1.3.1
    spdlogVersion=1.8.1
    googletest=1.10.x # recommend to use latest version
    ortoolsVersion=9.3

    # temp dir to download and compile
    baseDir=/tmp/installers
    mkdir -p "${baseDir}"

    # CMake
    if [[ -z $(cmake --version | grep ${cmakeVersionBig}) ]]; then
        cd "${baseDir}"
        $PROXYCHAINS wget https://cmake.org/files/v${cmakeVersionBig}/cmake-${cmakeVersionSmall}-Linux-x86_64.sh
        # md5sum -c <(echo "73041a43d27a30cdcbfdfdb61310d081  cmake-${cmakeVersionSmall}-Linux-x86_64.sh") || exit 1
        md5sum -c <(echo "53b3d1bbfb1f4d46213f338630ae7a30  cmake-${cmakeVersionSmall}-Linux-x86_64.sh") || exit 1
        $SUDO chmod +x cmake-${cmakeVersionSmall}-Linux-x86_64.sh
        ./cmake-${cmakeVersionSmall}-Linux-x86_64.sh --skip-license --prefix=/usr/local
    else
        echo "CMake already installed."
    fi

    # SWIG
    if [[ -z $(swig -version | grep ${swigVersion}) ]]; then
        cd "${baseDir}"
        $PROXYCHAINS wget https://github.com/swig/swig/archive/rel-${swigVersion}.tar.gz
        md5sum -c <(echo "ef6a6d1dec755d867e7f5e860dc961f7  rel-${swigVersion}.tar.gz") || exit 1
        tar xfz rel-${swigVersion}.tar.gz
        cd swig-rel-${swigVersion}
        ./autogen.sh
        ./configure --prefix=/usr
        make -j $(nproc)
        $SUDO make -j $(nproc) install
    else
        echo "Swig already installed."
    fi

    # boost
    if [[ -z $(grep "BOOST_LIB_VERSION \"${boostVersionBig//./_}\"" /usr/local/include/boost/version.hpp) ]]; then
        cd "${baseDir}"
        boostVersionUnderscore=${boostVersionSmall//./_}
        $PROXYCHAINS wget https://boostorg.jfrog.io/artifactory/main/release/${boostVersionSmall}/source/boost_${boostVersionUnderscore}.tar.gz
        md5sum -c <(echo "e425bf1f1d8c36a3cd464884e74f007a  boost_${boostVersionUnderscore}.tar.gz") || exit 1
        tar -xf boost_${boostVersionUnderscore}.tar.gz
        cd boost_${boostVersionUnderscore}
        ./bootstrap.sh
        $SUDO ./b2 install --with-iostreams --with-test --with-serialization --with-system --with-thread -j $(nproc)
    else
        echo "Boost already installed."
    fi

    # eigen
    if [[ ! -d /usr/local/include/eigen3/ ]]; then
        cd "${baseDir}"
        $PROXYCHAINS git clone -b ${eigenVersion} https://gitlab.com/libeigen/eigen.git
        cd eigen
        cmake -B build .
        cmake --build build -j $(nproc) --target install
    else
        echo "Eigen already installed."
    fi

    # lemon
    if [[ -z $(grep "LEMON_VERSION \"${lemonVersion}\"" /usr/local/include/lemon/config.h) ]]; then
        cd "${baseDir}"
        $PROXYCHAINS wget http://lemon.cs.elte.hu/pub/sources/lemon-${lemonVersion}.tar.gz
        md5sum -c <(echo "e89f887559113b68657eca67cf3329b5  lemon-${lemonVersion}.tar.gz") || exit 1
        tar -xf lemon-${lemonVersion}.tar.gz
        cd lemon-${lemonVersion}
        cmake -B build .
        cmake --build build -j $(nproc) --target install
    else
        echo "Lemon already installed."
    fi

    # spdlog
    if [[ -z $(grep "PACKAGE_VERSION \"${spdlogVersion}\"" ${spdlogFolder}) ]]; then
        cd "${baseDir}"
        $PROXYCHAINS git clone -b "v${spdlogVersion}" https://github.com/gabime/spdlog.git
        cd spdlog
        cmake -B build .
        cmake --build build -j $(nproc) --target install
    else
        echo "spdlog already installed."
    fi

    # gtest (latest)
    _installGtest

    _installProtobuf

    # or-tools (latest)
    cd "${baseDir}"
    $PROXYCHAINS git clone -b "v${ortoolsVersion}" https://github.com/google/or-tools
    cd or-tools
    cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS=ON
    cmake --build build -j $(nproc) --config Release -v 
    cmake --build build --target install

    cd "$lastDir"
    rm -rf "${baseDir}"
}

_installIncrement(){
    # temp dir to download and compile for increment
    baseDir=/tmp/installers_inc
    mkdir -p "${baseDir}"

    case "${package_name}" in
        gtest|gmock|googletest)
            _installGtest
            ;;
        *)
            echo "did not find package: ${package_name}"
            _help
            ;;
    esac

    cd "${baseDir}/.."
    rm -rf "${baseDir}"
    exit 1
}

_installUbuntuCleanUp() {
    apt-get autoclean -y
    apt-get autoremove -y
}

_installUbuntuDev() {
    export DEBIAN_FRONTEND="noninteractive"
    apt-get -y update
    apt-get -y install tzdata
    apt-get -y install \
        automake \
        autotools-dev \
        bison \
        flex \
        clang \
        g++ \
        gcc \
        git \
        lcov \
        libpcre3-dev \
        python3-dev \
        libreadline-dev \
        tcl-dev \
        tcllib \
        wget \
        zlib1g-dev \
        liboctave-dev \
        libfftw3-dev
}

_installUbuntuRuntime() {
    export DEBIAN_FRONTEND="noninteractive"
    apt-get -y update
    apt-get -y install tzdata
    apt-get install -y \
        binutils \
        libgomp1 \
        libpython3.8 \
        libtcl \
        qt5-default \
        qt5-image-formats-plugins \
        tcl-tclreadline
    # need the strip "hack" above to run on docker
    strip --remove-section=.note.ABI-tag /usr/lib/x86_64-linux-gnu/libQt5Core.so
}

_installCentosCleanUp() {
    yum clean -y all
    rm -rf /var/lib/apt/lists/*
}

_installCentosDev() {
    if [[ -z $(yum list installed lcov) ]]; then
        yum install -y http://downloads.sourceforge.net/ltp/lcov-1.14-1.noarch.rpm
    fi
    if [[ -z $(yum list installed ius-release) ]]; then
        yum install -y https://repo.ius.io/ius-release-el7.rpm
    fi
    yum groupinstall -y "Development Tools"
    yum install -y centos-release-scl
    yum install -y \
        devtoolset-8 \
        devtoolset-8-libatomic-devel \
        libstdc++ \
        llvm-toolset-7.0 \
        llvm-toolset-7.0-libomp-devel \
        pcre-devel \
        readline-devel \
        tcl \
        tcl-devel \
        tcllib \
        tcl-tclreadline-devel \
        zlib-devel \
        wget
    yum install -y \
        python-devel \
        python36 \
        python36-devel \
        python36-pip
}

_installCentosRuntime() {
    yum update -y
    if [[ -z $(yum list installed epel-release) ]]; then
        yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
    fi
    yum install -y \
        libgomp \
        python36-libs \
        qt5-qtbase-devel \
        qt5-qtimageformats \
        tcl-tclreadline
    yum update -y
}

_help() {
    cat <<EOF

Usage: $0 -run[time]
       $0 -dev[elopment]
       $0 -incr[ement] package_name 
                package list:
                    gtest|gmock|googletest) => googletest framwork
EOF
    exit "${1:-1}"
}

# default option
option="runtime"

# default values, can be overwritten by cmdline args
while [ "$#" -gt 0 ]; do
    case "${1}" in
        -h|-help)
            _help 0
            ;;
        -run|-runtime)
            option="runtime"
            ;;
        -dev|-development)
            option="dev"
            ;;
        -local_build)
            SUDO="sudo"
            ;;
        -proxychains)
            PROXYCHAINS="proxychains"
            ;;
        -incr|-increment)
            if [ $# -gt 1 ]; then
                package_name=${2}
                _installIncrement
            else
                echo "-increment: need a package name after"
                _help
            fi
            ;;
        *)
            echo "unknown option: ${1}" >&2
            _help
            ;;
    esac
    shift 1
done

platform="$(uname -s)"
case "${platform}" in
    "Linux" )
        if [[ -f /etc/os-release ]]; then
            os=$(awk -F= '/^NAME/{print $2}' /etc/os-release | sed 's/"//g')
        else
            os="Unidentified OS, could not find /etc/os-release."
        fi
        ;;
    *)
        echo "${platform} is not supported" >&2
        echo "We only officially support Linux at the moment." >&2
        _help
        ;;
esac

case "${os}" in
    "CentOS Linux" )
        spdlogFolder="/usr/local/lib64/cmake/spdlog/spdlogConfigVersion.cmake"
        export spdlogFolder
        _installCentosRuntime
        if [[ "${option}" == "dev" ]]; then
            _installCentosDev
            _installCommonDev
        fi
        _installCentosCleanUp
        cat <<EOF
To enable GCC-8 or Clang-7 you need to run:
    source /opt/rh/devtoolset-8/enable
    source /opt/rh/llvm-toolset-7.0/enable
EOF
        ;;
    "Ubuntu" )
        spdlogFolder="/usr/local/lib/cmake/spdlog/spdlogConfigVersion.cmake"
        export spdlogFolder
        _installUbuntuRuntime
        if [[ "${option}" == "dev" ]]; then
            _installUbuntuDev
            _installCommonDev
        fi
        _installUbuntuCleanUp
        ;;
    *)
        echo "unsupported system: ${os}" >&2
        echo "supported systems are CentOS 7 and Ubuntu 20.04" >&2
        _help
        ;;
esac
