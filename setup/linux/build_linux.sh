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
rootDir=$(cd "${currentdir}/../.."; pwd)

echo "build directory: ${currentdir}."
echo "root directory: ${rootDir}."

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

# 默认查找编译产物目录（优先使用环境变量 BUILD_FILE_DIR，次之 output-$ARCH_DIR/screenSaver，再次之 setup/linux/build）
if [ -z "${BUILD_FILE_DIR}" ]; then
  if [ -d "${rootDir}/output-${ARCH_DIR}/screenSaver" ]; then
    BUILD_FILE_DIR="${rootDir}/output-${ARCH_DIR}/screenSaver"
  elif [ -d "${currentdir}/build" ]; then
    BUILD_FILE_DIR="${currentdir}/build"
  fi
fi
echo "build files directory: ${BUILD_FILE_DIR}"

# 创建临时打包目录，退出时自动清理，不污染原始仓库
STAGE_DIR=$(mktemp -d /tmp/screenSaver_pkg_${ARCH_DIR}_XXXXXX 2>/dev/null || mktemp -d "${rootDir}/.tmp_${ARCH_DIR}_XXXXXX")
trap "rm -rf '$STAGE_DIR'" EXIT INT TERM

projectDir="${STAGE_DIR}/pkg_root/opt/apps/com.sinoparasoft.screenSaver/files"

function init() 
{
  mkdir -p "$STAGE_DIR/pkg_root"
  mkdir -p "$projectDir"

  # 复制模板文件到临时打包目录，不污染原始仓库
  if [ -d "$currentdir/$ARCH_DIR" ]; then
    cp -r "$currentdir/$ARCH_DIR"/* "$STAGE_DIR/pkg_root/" 2>/dev/null || true
  fi
  if [ -d "$currentdir/common" ]; then
    cp -r "$currentdir/common"/* "$STAGE_DIR/pkg_root/" 2>/dev/null || true
  fi

  # 在临时目录下生成 ver 版本信息
  cur_time=$(date "+%Y-%m-%d %H:%M:%S")
  rm -f "${projectDir}/ver" &> /dev/null
  cat <<EOF > "${projectDir}/ver"
[info]
build_version=${BUILD_NUM}
build_time=$cur_time
version=${version}
vendor=sinoparasoft
EOF

  # 在临时目录下更新 control 和 info
  if [ -f "$STAGE_DIR/pkg_root/DEBIAN/control" ]; then
    sed -i 's/^Version:.*/Version: '"$version"'/g' "$STAGE_DIR/pkg_root/DEBIAN/control"
    sed -i 's/^Architecture:.*/Architecture: '"$ARCH_DIR"'/g' "$STAGE_DIR/pkg_root/DEBIAN/control"
  fi

  if [ -f "$STAGE_DIR/pkg_root/opt/apps/com.sinoparasoft.screenSaver/info" ]; then
    sed -i 's/"version":.*/"version": "'"$version"'",/g' "$STAGE_DIR/pkg_root/opt/apps/com.sinoparasoft.screenSaver/info"
  fi
}

function prepare_files()
{
  echo -e "\ncopy files from ${BUILD_FILE_DIR} to ${projectDir}"
  if [ -n "${BUILD_FILE_DIR}" ] && [ -d "${BUILD_FILE_DIR}" ]; then
    cp -rf ${BUILD_FILE_DIR}/* "${projectDir}/" &> /dev/null || true
  fi

  if [ -d "$STAGE_DIR/pkg_root/DEBIAN" ]; then
    chmod -R 0755 "$STAGE_DIR/pkg_root/DEBIAN"
  fi
  if [ -d "$STAGE_DIR/pkg_root/lib/systemd/system" ]; then
    chmod 0644 "$STAGE_DIR/pkg_root/lib/systemd/system"/* 2>/dev/null || true
  fi
}

function update_and_compress() {
  local PACKAGE_VERSION="$1"
  local ARCHITECT="$2"

  local JSON_FILE="$STAGE_DIR/abstract.json"
  local PACKAGE_NAME="com.sinoparasoft.screenSaver_${PACKAGE_VERSION}_${ARCHITECT}.deb"

  if [ -f "$currentdir/abstract.json" ]; then
    cp -f "$currentdir/abstract.json" "$JSON_FILE"
  else
    cat <<EOF > "$JSON_FILE"
{
  "PACKAGE_NAME": "${PACKAGE_NAME}",
  "PACKAGE_VERSION": "${PACKAGE_VERSION}",
  "PACKAGE_DESCRIBE": "screenSaver",
  "ARCHITECT": "${ARCHITECT}",
  "OS_VERSION": "linux"
}
EOF
  fi

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
  cd "$STAGE_DIR"
  zip -r "$ZIP_NAME" "$PACKAGE_NAME" "abstract.json" > /dev/null

  if [ $? -eq 0 ]; then
    echo "Created ZIP archive: $ZIP_NAME"
  else
    echo "Failed to create ZIP archive."
    return 1
  fi
}

function make_package()
{
  local DEB_NAME="com.sinoparasoft.screenSaver_${version}_${ARCH_DIR}.deb"
  local ZIP_NAME="com.sinoparasoft.screenSaver_${version}_${ARCH_DIR}.zip"

  dpkg -b "$STAGE_DIR/pkg_root" "$STAGE_DIR/$DEB_NAME"
  if [ $? -ne 0 ]; then
    echo "dpkg -b failed!"
    return 1
  fi

  # 生成zip
  update_and_compress "$version" "$ARCH_DIR"

  # 复制生成的二进制包到项目根目录
  if [ -f "$STAGE_DIR/$DEB_NAME" ]; then
    cp -f "$STAGE_DIR/$DEB_NAME" "$rootDir/"
    echo "Copied $DEB_NAME to project root: $rootDir"
  fi

  if [ -f "$STAGE_DIR/$ZIP_NAME" ]; then
    cp -f "$STAGE_DIR/$ZIP_NAME" "$rootDir/"
    echo "Copied $ZIP_NAME to project root: $rootDir"
  fi
}

# make setup package.
if [ $# -ge 2 ] && [ $2 = "-a" ]; then
  echo 'make_package'
  init
  prepare_files
  make_package
fi

echo -e "all is ok !"
exit 0
