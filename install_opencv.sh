#!/bin/bash

# Reference https://learnopencv.com/install-opencv-3-4-4-on-ubuntu-18-04/
echo "OpenCV installation by learnOpenCV.com"

sudo apt install -y build-essential cmake git pkg-config libgtk-3-dev \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
    libxvidcore-dev libx264-dev libjpeg-dev libpng-dev libtiff-dev \
    gfortran openexr libatlas-base-dev python3-dev python3-numpy \
    libtbb2 libtbb-dev libdc1394-22-dev libopenexr-dev \
    libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev

mkdir ~/opencv_build && cd ~/opencv_build

# OpenCV
git clone https://github.com/opencv/opencv.git
cd opencv
git checkout 3.4
cd ..

# OpenCV contribution
git clone https://github.com/opencv/opencv_contrib.git
cd opencv_contrib
git checkout 3.4
cd ..

cd ~/opencv_build/opencv
mkdir -p build && cd build

cmake -D CMAKE_BUILD_TYPE=RELEASE \
	-D CMAKE_INSTALL_PREFIX=/usr/local \
	-D INSTALL_C_EXAMPLES=ON \
	-D INSTALL_PYTHON_EXAMPLES=ON \
	-D OPENCV_GENERATE_PKGCONFIG=ON \
        -D OPENCV_EXTRA_MODULES_PATH=~/opencv_build/opencv_contrib/modules \
        -D BUILD_EXAMPLES=ON ..

make -j4
make install

# To verify OpenCV version
pkg-config --modversion opencv3
python3 -c "import cv2; print(cv2.__version__)"
