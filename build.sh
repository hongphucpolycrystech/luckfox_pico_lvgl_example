#!/bin/bash
#echo "📁 Tạo thư mục build..."
# Thư mục build
BUILD_DIR="build"
export LUCKFOX_SDK_PATH="/home/luckfox/luckfox-pico/"
export STAGING_DIR_PATH="/home/luckfox/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/staging/"
# Nếu chưa có thư mục build thì tạo và chạy cmake
if [ ! -d "$BUILD_DIR" ]; then
    echo "📁 Tạo thư mục build..."
    mkdir "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Nếu dùng toolchain cross-compile:
    # cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake

    # Native build:
    echo "⚙️  Chạy cmake lần đầu..."
    cmake ..
else
    echo "📁 Đã có thư mục build, chuyển vào..."
    cd "$BUILD_DIR"
fi

# Dùng toolchain CMake nếu có
# Uncomment dòng sau nếu bạn có toolchain.cmake
# TOOLCHAIN_FILE=../toolchain.cmake
# cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE
#set(LUCKFOX_SDK_PATH "/home/luckfox/luckfox-pico/")


# Hoặc dùng CMake mặc định (native build)
echo "⚙️  Chạy CMake..."
cmake ..

echo "🔨 Biên dịch..."
make -j$(nproc)

# Tuỳ chọn cài đặt ra thư mục riêng
echo "📦 Cài đặt vào output/"
make install DESTDIR=output

echo "✅ Build hoàn tất. File nằm trong: $BUILD_DIR/myapp"
echo "copy to board"
# sudo scp ./output/usr/local/bin/luckfox_lvgl admin:///shared/
sudo cp ./output/usr/local/bin/ircam  /media
sudo sshpass -p "luckfox" scp ./output/usr/local/bin/ircam root@192.168.1.177:/root
echo "copy to board done"
