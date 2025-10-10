#!/bin/bash
echo "Installing Chart Preview plugins..."
cp -R ChartPreview.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R ChartPreview.component ~/Library/Audio/Plug-Ins/Components/
echo "Installation complete!"
echo "VST3 installed to: ~/Library/Audio/Plug-Ins/VST3/"
echo "AU installed to: ~/Library/Audio/Plug-Ins/Components/"
