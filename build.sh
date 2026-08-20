#!/usr/bin/env bash

if [ $# -eq 0 ]; then
	echo "USAGE: $0 [amd64|arm64|mips64|loongarch64] buildNum branch"
	exit 1
fi

basepath=$(dirname $(readlink -f "$0"))
echo $basepath

# init
ARCH=$1
BUILD_NUM=$2
BRANCH=$3
export BUILD_NUM=$BUILD_NUM
mkdir -p $basepath/build-$ARCH
mkdir -p $basepath/output-$ARCH
mkdir -p $basepath/deps

export BOOST_ROOT=/opt/boost_1_87_0

download_product() {
    local package_name=$1
    local arch=$2
    local branch=$3
    local base_url="http://nexus.zkjs.com/repository/rawhostedrepo/product/${branch}/${package_name}/${arch}"
    local version_file="ver.txt"
    local build_number
    local product_file
    local product_url

    if [ -z "${package_name}" ] || [ -z "${arch}" ]; then
        echo "Usage: download_product <package_name> <arch>"
        return 1
    fi

    echo "Downloading version file..."
    if ! wget -q "${base_url}/${version_file}" -O "${version_file}"; then
        echo "Failed to download version file."
        return 1
    fi

    build_number=$(grep -oP '^\d+' "${version_file}")
    if [ -z "${build_number}" ]; then
        echo "Failed to parse build number from version file."
        return 1
    fi
    rm ${version_file}
    echo "Detected build number: ${build_number}"

    product_file="${package_name}-${build_number}-${arch}.tar.gz"
    product_url="${base_url}/${build_number}/${product_file}"

    echo "Downloading product file: ${product_file}..."
    if ! wget -q "${product_url}" -O "${product_file}"; then
        echo "Failed to download product file."
        return 1
    fi

    echo "Download complete: ${product_file}"
    tar -xzf "$product_file" -C "$basepath/deps"
    if [ $? -eq 0 ]; then
        echo "Extracted $product_file to $basepath/deps."
    else
        echo "Failed to extract $product_file."
    fi
    rm -f "$product_file"

    return 0
}

pushd $basepath/build-$ARCH
cmake .. -DARCH_NAME=$ARCH -DCMAKE_INSTALL_PREFIX=$basepath/output-$ARCH
RET=$?
if [ $RET -ne 0 ]; then
    echo "cmake 配置失败，退出。"
    popd
    exit $RET
fi

make -j$(nproc)
RET=$?
if [ $RET -ne 0 ]; then
    echo "make 构建失败，退出。"
    popd
    exit $RET
fi
echo "构建成功！"
make install

#\cp $basepath/deps/* $basepath/output-$ARCH/ -af
#cp $basepath/app.json $basepath/output-$ARCH/bin/app.json
mkdir $basepath/output-$ARCH/bin/lib -p

if [ "$ARCH" == "amd64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_thread.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_program_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
elif [ "$ARCH" == "arm64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_program_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
#    cp /opt/boost_1_87_0/lib/libboost_thread_options.so.1.87.0 $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
elif [ "$ARCH" == "mips64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
elif [ "$ARCH" == "loongarch64" ]; then
    cp /opt/boost_1_87_0/lib/libboost_system.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_filesystem.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_process.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_date_time.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_context.so.1.87.0 $basepath/output-$ARCH/bin/lib
    cp /opt/boost_1_87_0/lib/libboost_atomic.so.1.87.0 $basepath/output-$ARCH/bin/lib
    rm -rf $basepath/output-$ARCH/lib
    rm -rf $basepath/output-$ARCH/include
fi
mv $basepath/output-$ARCH/bin $basepath/output-$ARCH/screenSaver
popd

echo "Calling setup/linux/build_linux.sh for packaging..."
export BUILD_FILE_DIR="$basepath/output-$ARCH/screenSaver"
bash $basepath/setup/linux/build_linux.sh $ARCH -a $BUILD_NUM "$BRANCH"
RET=$?
if [ $RET -ne 0 ]; then
    echo "Linux build_linux.sh failed!"
    exit $RET
fi

echo "Linux build and packaging completed successfully!"
exit 0