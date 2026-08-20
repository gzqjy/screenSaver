#!/bin/bash

# show usage.
if [ $# -eq 0 ]; then
  echo "USAGE: $0 [amd64|arm64|mips64|loongarch64] -a  buildNum branch"
  exit 1
fi

# show build environment.
version="1.1."
if [ $# -ge 2 ]; then
  version=${version}$3  
fi
echo "build version: ${version}"
currentdir=$(cd "$(dirname "$0")"; pwd)

echo "build directory: ${currentdir}."

# echo "[Download] public lib"
# todo

BRANCH=$4
echo "branch: ${BRANCH}"

# prepare source code.
if [ $1 = "arm64" ]; then
  ARCH_DIR="arm64"
  arch="arm64"
  upgrade_arch="arm64/aarch64"
elif [ $1 = "amd64" ]; then
  ARCH_DIR="amd64"
  arch="amd64"
  upgrade_arch="amd64/x86_64"
elif [ $1 = "mips64" ]; then
  ARCH_DIR="mips64"
  arch="mips64el"
  upgrade_arch="mips64el/mips64"
elif [ $1 = "loongarch64" ]; then
  ARCH_DIR="loongarch64"
  arch="loongarch64"
  upgrade_arch="loongarch64"
else
  echo "unknown ARCH."
  exit 1
fi
echo "build arch: $1 ${ARCH_DIR}."

packagesDir=$3
projectDir=${currentdir}/${ARCH_DIR}/opt/apps/com.sinoparasoft.screenSaver/files

# build product
BUILD_FILE_DIR="${currentdir}/build/"


function  init() 
{
  #write version
  cur_time=$(date "+%Y-%m-%d %H:%M:%S")
  rm ${projectDir}/ver    &> /dev/null
  echo "[info]" >> ${projectDir}/ver
  echo "build_version=${BUILD_NUM}" >> ${projectDir}/ver
  echo "build_time=$cur_time" >> ${projectDir}/ver
  echo "version=${version}" >> ${projectDir}/ver
  echo "vendor=sinoparasoft" >> ${projectDir}/ver

  #设置RPM包 软件版本号
  #sed -i 's/Release:.*/Release: '$3'/g' $currentdir/com.sinoparasoft.screenSaver/files.spec
  #sed -i 's/Release:.*/Release: '$3'/g' $currentdir/com.sinoparasoft.screenSaver/files_loongarch64.spec

  sed -i 's/^Version:.*/Version: '"$version"'/g' $currentdir/common/DEBIAN/control
  sed -i 's/^Architecture:.*/Architecture: '"$ARCH_DIR"'/g' $currentdir/common/DEBIAN/control

  sed -i 's/version\":.*/version\": '\"$version\",'/g' $currentdir/$ARCH_DIR/opt/apps/com.sinoparasoft.screenSaver/info
}

download_product() {
    local package_name=$1
    local local_arch=$2
    local download_dir=$3
    local branch=$4
    local base_url="http://nexus.zkjs.com/repository/rawhostedrepo/product/${package_name}/${local_arch}"
    local version_file="ver.txt"
    local build_number
    local product_file
    local product_url
    
    if [ -n "${branch}" ]; then
        base_url="http://nexus.zkjs.com/repository/rawhostedrepo/product/${branch}/${package_name}/${local_arch}"
    fi

    echo "package_name=${package_name}"
    echo "arch=${local_arch}"
    echo "base_usr = ${base_url}"

    if [ -z "${package_name}" ] || [ -z "${arch}" ]; then
        echo "Usage: download_product <package_name> <arch>"
        return 1
    fi

    echo "Downloading version file from ${base_url}/${version_file}"
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
    tar -xzf "$product_file" -C "$download_dir"
    if [ $? -eq 0 ]; then
        echo "Extracted $product_file to $download_dir."
    else
        echo "Failed to extract $product_file."
    fi
    rm -f "$product_file"

    return 0
}

download_and_extract() {
    local base_url="$1"
    local file="$2"
    local target_dir="$3"

    if [ -z "$base_url" ] || [ -z "$file" ] || [ -z "$target_dir" ]; then
        echo "Usage: download_and_extract <base_url> <file_name> <target_directory>"
        return 1
    fi

    mkdir -p "$target_dir"

    echo "Downloading $file..."
    wget -q "${base_url}${file}" -O "$file"
    if [ $? -eq 0 ]; then
        echo "Successfully downloaded $file. Extracting..."
        tar -xzf "$file" -C "$target_dir"
        if [ $? -eq 0 ]; then
            echo "Extracted $file to $target_dir."
        else
            echo "Failed to extract $file."
        fi
        # 删除下载的压缩包（可选）
        rm -f "$file"
    else
        echo "Failed to download $file."
        return 1
    fi

    echo "Task completed."
}

function prepare_files()
{
	
  echo -e "\ncopy files from ${BUILD_FILE_DIR} to  ${projectDir}"
  cp -rf ${BUILD_FILE_DIR}/* ${projectDir}  &> /dev/null

  chmod -R 0755     $currentdir/common/DEBIAN
  chmod 0644    $currentdir/common/lib/systemd/system/*

  cp -rf $currentdir/common/* $currentdir/$ARCH_DIR/

  # download component
  cd $currentdir

  ## modules
  #mkdir $currentdir/$ARCH_DIR/opt/apps/com.sinoparasoft.screenSaver/files/modules

  ### mediummanager
  #download_product "screenSaver" "${ARCH_DIR}" "$currentdir/$ARCH_DIR/opt/apps/com.sinoparasoft.screenSaver/files/modules" "${BRANCH}"
  #mv $currentdir/$ARCH_DIR/opt/apps/com.sinoparasoft.screenSaver/files/modules/mediummanager/MediumManager $currentdir/$ARCH_DIR/opt/apps/com.sinoparasoft.screenSaver/files/modules/mediummanager/MediumIssue
}

function check_build_files(){
  if [ ! -f "${projectDir}/modules/mediummanager/MediumIssue" ];then
    echo "no MediumIssue"
    return -1;
  fi

  return 0;
}

function update_and_compress() {
  cd $currentdir

  local PACKAGE_VERSION="$1"
  local ARCHITECT="$2"

  local JSON_FILE="abstract.json"
  local PACKAGE_NAME="com.sinoparasoft.screenSaver_${PACKAGE_VERSION}_${ARCHITECT}.deb"

  sed -i \
      -e "s#\"PACKAGE_NAME\": \"[^\"]*\"#\"PACKAGE_NAME\": \"$PACKAGE_NAME\"#" \
      -e "s#\"PACKAGE_VERSION\": \"[^\"]*\"#\"PACKAGE_VERSION\": \"$PACKAGE_VERSION\"#" \
      -e "s#\"ARCHITECT\": \"[^\"]*\"#\"ARCHITECT\": \"$ARCHITECT\"#" \
      "$JSON_FILE"
  
  if [ $? -ne 0 ]; then
    echo "Failed to update $JSON_FILE."
    return 1
  fi

  echo "Updated $JSON_FILE successfully."

  local ZIP_NAME="com.sinoparasoft.screenSaver_${PACKAGE_VERSION}_${ARCHITECT}.zip"
  zip -r "$ZIP_NAME" "$PACKAGE_NAME" "$JSON_FILE" > /dev/null

  if [ $? -eq 0 ]; then
    echo "Created ZIP archive: $ZIP_NAME"
  else
    echo "Failed to create ZIP archive."
    return 1
  fi
}

function make_package()
{
  cd $currentdir

  dpkg -b $currentdir/$ARCH_DIR   $currentdir/

  # 生成zip
  update_and_compress "$version" "$ARCH_DIR"
  
  # echo "build rpm package..."
  if [ $ARCH_DIR = "amd64" ]; then
    echo "amd64 rpm todo..."
    #ls  -al $currentdir/$ARCH_DIR/*
    #mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS} 
    #echo  "$currentdir/$ARCH_DIR/*  ->  ~/rpmbuild/BUILD/"
    #cp -ar  $currentdir/$ARCH_DIR/*  ~/rpmbuild/BUILD/
    #echo "after cp ------"
    #ls  -al ~/rpmbuild/BUILD/
    #rm -rf  ~/rpmbuild/BUILD/DEBIAN
    #rpmbuild -bb ./com.sinoparasoft.screenSaver.spec
    #cp ~/rpmbuild/RPMS/x86_64/*.rpm ./
  elif [ $ARCH_DIR = "arm64" ]; then
    echo "arm64 rpm todo..."
  elif [ $ARCH_DIR = "mips64" ]; then
    echo "mips64 rpm todo..."
  elif [ $ARCH_DIR = "loongarch64" ]; then
    echo "loongarch64 rpm todo..."
  fi

}

# make setup package.
if [ $# -ge 2 ] && [ $2 = "-a" ]; then
  echo 'make_package'
  init
  prepare_files
#  check_build_files
#  if [ $? -ne 0 ]; then
#    echo "check_build_files failed!"
#    exit 1;
#  fi
  make_package
fi

echo -e "all is ok !"
exit 0


