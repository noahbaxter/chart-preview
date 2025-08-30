#!/bin/bash

# Define the variables
build_type=$1
config_type=$2

build_type="vst"
# build_type="standalone"
config_type="Debug"

# Navigate to the project directory
cd ../Builds/MacOSX

# xcodebuild clean -project ChartPreview.xcodeproj

# Check the build type
if [ "$build_type" == "standalone" ]; then
    osascript -e 'quit app "ChartPreview"'

    xcodebuild -quiet -project ChartPreview.xcodeproj -target "ChartPreview - Standalone Plugin" -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        open build/$config_type/ChartPreview.app
    fi
elif [ "$build_type" == "vst" ]; then
    # osascript -e 'quit app "Ableton Live 12 Suite"'
    osascript -e 'quit app "REAPER"'
    
    # Check if Projucer file exists and is newer than xcodeproj
    if [ -f "../../ChartPreview.jucer" ] && [ "../../ChartPreview.jucer" -nt "ChartPreview.xcodeproj" ]; then
        echo "Regenerating Xcode project from Projucer..."
        "/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "../../ChartPreview.jucer"
    fi
    xcodebuild -quiet -project ChartPreview.xcodeproj -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        cp -R build/$config_type/ChartPreview.vst3 ~/Library/Audio/Plug-Ins/VST3/

        # open -a "Ableton Live 12 Suite" "/Users/noahbaxter/Code/personal/chart-preview/ableton-test Project/ableton-test.als"
        open -a "REAPER" "/Users/noahbaxter/Code/personal/chart-preview/reaper-test/reaper-test.RPP"
    fi
elif [ "$build_type" == "au" ]; then
    xcodebuild -quiet -project ChartPreview.xcodeproj -configuration $config_type
    if [ $? -eq 0 ]; then
        echo "SUCCESS"
        cp -R build/$config_type/ChartPreview.component ~/Library/Audio/Plug-Ins/Components/
    fi
fi