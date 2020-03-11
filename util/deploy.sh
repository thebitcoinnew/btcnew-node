#!/bin/bash
./util/build_prep/update-common && ./util/build_prep/deploynode/prep.sh
BOOST_ROOT=/usr/local/boost

set -o xtrace

DISTRO_CFG=""
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    CPACK_TYPE="TBZ2"
    distro=$(lsb_release -i -c -s|tr '\n' '_')
    DISTRO_CFG="-DBTCNEW_DISTRO_NAME=${distro}"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CPACK_TYPE="DragNDrop"
elif [[ "$OSTYPE" == "cygwin" ]]; then
    CPACK_TYPE="TBZ2"
elif [[ "$OSTYPE" == "msys" ]]; then
    CPACK_TYPE="NSIS" #?
elif [[ "$OSTYPE" == "win32" ]]; then
    CPACK_TYPE="NSIS"
elif [[ "$OSTYPE" == "freebsd"* ]]; then
    CPACK_TYPE="TBZ2"
    DISTRO_CFG="-DBTCNEW_DISTRO_NAME='freebsd'"
else
    CPACK_TYPE="TBZ2"
fi

if [[ ${SIMD} -eq 1 ]]; then
    SIMD_CFG="-DBTCNEW_SIMD_OPTIMIZATIONS=ON"
    echo SIMD and other optimizations enabled
    echo local CPU:
    cat /proc/cpuinfo # TBD for macOS
else
    SIMD_CFG=""
fi

if [[ ${ASAN_INT} -eq 1 ]]; then
    SANITIZERS="-DBTCNEW_ASAN_INT=ON"
elif [[ ${ASAN} -eq 1 ]]; then
    SANITIZERS="-DBTCNEW_ASAN=ON"
elif [[ ${TSAN} -eq 1 ]]; then
    SANITIZERS="-DBTCNEW_TSAN=ON"
else
    SANITIZERS=""
fi

if [[ "${BOOST_ROOT}" -ne "" ]]; then
    BOOST_CFG="-DBOOST_ROOT='${BOOST_ROOT}'"
else
    BOOST_CFG=""
fi

BUSYBOX_BASH=${BUSYBOX_BASH-0}
if [[ ${FLAVOR-_} == "_" ]]; then
    FLAVOR=""
fi

if [[ "${BETA}" -eq 1 ]]; then
    NETWORK_CFG="-DACTIVE_NETWORK=btcnew_beta_network"
    CONFIGURATION="RelWithDebInfo"
else
    NETWORK_CFG="-DACTIVE_NETWORK=btcnew_live_network"
    CONFIGURATION="Release"
fi

set -o nounset

run_build() {
    build_dir=build_${FLAVOR}

    mkdir ${build_dir}
    cd ${build_dir}
    cmake -GNinja \
       -DBOOST_ROOT=/usr/local/boost \
       -DCMAKE_BUILD_TYPE=${CONFIGURATION} \
       -DCMAKE_VERBOSE_MAKEFILE=ON \
       ${NETWORK_CFG} \
       ${DISTRO_CFG} \
       ${SIMD_CFG} \
       ${BOOST_CFG} \
       ${SANITIZERS} \
       ..

    cmake --build ${PWD} -- -j$(nproc) -v
    cmake --build ${PWD} -- install -v
    cp ${PWD}/btcnew_node /usr/local/bin/btcnew_node
    cp ${PWD}/btcnew_rpc /usr/local/bin/btcnew_rpc
    ./btcnew_node --generate_config node > ~/BitcoinNew/config-node.toml
}

run_build
