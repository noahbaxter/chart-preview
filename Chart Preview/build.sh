#!/bin/bash

# Define the variables
build_type=$1
config_type=$2

build_type="vst"
# build_type="standalone"
config_type="Debug"

# Navigate to the project directory
cd Builds/MacOSX

# xcodebuild clean -project RBN_prev.xcodeproj

# Check the build type
if [ "$build_type" == "standalone" ]; then
    osascript -e 'quit app "RBN_prev"'

    xcodebuild -quiet -project RBN_prev.xcodeproj -target "RBN_prev - Standalone Plugin" -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        open build/$config_type/RBN_prev.app
    fi
elif [ "$build_type" == "vst" ]; then
    osascript -e 'quit app "Ableton Live 12 Suite"'

    xcodebuild -quiet -project RBN_prev.xcodeproj -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        cp -R build/$config_type/RBN_prev.vst3 ~/Library/Audio/Plug-Ins/VST3/

        open -a "Ableton Live 12 Suite" "/Users/noahbaxter/Code/personal/chart-preview/ableton-test Project/ableton-test.als"
    fi
elif [ "$build_type" == "au" ]; then
    xcodebuild -quiet -project RBN_prev.xcodeproj -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        cp -R build/$config_type/RBN_prev.component ~/Library/Audio/Plug-Ins/Components/
    fi
fi