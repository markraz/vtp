#!/bin/sh

if [ $# -lt 1 ] ; then
  echo "Usage: mk_vtp_src_zips.sh date"
  echo
  echo "Example: mk_vtp_src_zips.sh 021029"
  exit
fi

TARGETDIR=C:/Distrib
DATE=$1
DIST_FILE1=${TARGETDIR}/vtp-src-${DATE}.zip
DIST_FILE2=${TARGETDIR}/vtp-srcdocs-${DATE}.zip

# Create the archive containing the Source
rm -f $DIST_FILE1

zip $DIST_FILE1 VTP/TerrainSDK/configure
zip $DIST_FILE1 VTP/TerrainSDK/configure.in
zip $DIST_FILE1 VTP/TerrainSDK/install-sh
zip $DIST_FILE1 VTP/TerrainSDK/Make.defs
zip $DIST_FILE1 VTP/TerrainSDK/Makedefs.in
zip $DIST_FILE1 VTP/TerrainSDK/Makefile
zip $DIST_FILE1 VTP/TerrainSDK/Makefile.in
zip $DIST_FILE1 VTP/TerrainSDK/vtdata/*
zip $DIST_FILE1 VTP/TerrainSDK/vtdata/boost/*
zip $DIST_FILE1 VTP/TerrainSDK/vtdata/shapelib/*

zip $DIST_FILE1 VTP/TerrainSDK/vtlib/*
zip $DIST_FILE1 VTP/TerrainSDK/vtlib/core/*
zip $DIST_FILE1 VTP/TerrainSDK/vtlib/vtosg/*
zip $DIST_FILE1 VTP/TerrainSDK/vtlib/vtsgl/*

zip $DIST_FILE1 VTP/TerrainSDK/xmlhelper/*
zip $DIST_FILE1 VTP/TerrainSDK/xmlhelper/include/*

zip $DIST_FILE1 VTP/TerrainSDK/vtui/*

zip $DIST_FILE1 VTP/TerrainApps/configure
zip $DIST_FILE1 VTP/TerrainApps/configure.in
zip $DIST_FILE1 VTP/TerrainApps/install-sh
zip $DIST_FILE1 VTP/TerrainApps/Make.defs
zip $DIST_FILE1 VTP/TerrainApps/Makedefs.in
zip $DIST_FILE1 VTP/TerrainApps/Makefile
zip $DIST_FILE1 VTP/TerrainApps/Makefile.in
zip $DIST_FILE1 VTP/TerrainApps/README.sgi
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/license.txt
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/*.h
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/BExtractor.dsp
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/BExtractor.dsw
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/BExtractor.rc
zip $DIST_FILE1 VTP/TerrainApps/BExtractor/res/*

zip $DIST_FILE1 VTP/TerrainApps/CManager/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/CManager/*.h
zip $DIST_FILE1 VTP/TerrainApps/CManager/CManager.dsp
zip $DIST_FILE1 VTP/TerrainApps/CManager/CManager-OSG.dsw
zip $DIST_FILE1 VTP/TerrainApps/CManager/CManager.rc
zip $DIST_FILE1 VTP/TerrainApps/CManager/itemtypes.txt
zip $DIST_FILE1 VTP/TerrainApps/CManager/cmanager.wdr
zip $DIST_FILE1 VTP/TerrainApps/CManager/bitmaps/*
zip $DIST_FILE1 VTP/TerrainApps/CManager/icons/*

zip $DIST_FILE1 VTP/TerrainApps/Enviro/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/Enviro/*.h
zip $DIST_FILE1 VTP/TerrainApps/Enviro/Enviro.ini
zip $DIST_FILE1 VTP/TerrainApps/Enviro/license.txt
zip $DIST_FILE1 VTP/TerrainApps/Enviro/Makefile
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wxEnviro.dsp
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wxEnviro-OSG.dsw
zip $DIST_FILE1 VTP/TerrainApps/Enviro/mfcEnviro.dsp
zip $DIST_FILE1 VTP/TerrainApps/Enviro/mfcEnviro-OSG.dsw
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/*.h
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/enviro-wx.rc
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/enviro.wdr
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/bitmap/*
zip $DIST_FILE1 VTP/TerrainApps/Enviro/wx/icons/*
zip $DIST_FILE1 VTP/TerrainApps/Enviro/mfc/*
zip $DIST_FILE1 VTP/TerrainApps/Enviro/mfc/res/*

zip $DIST_FILE1 VTP/TerrainApps/glutSimple/app.cpp
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/glutSimple.dsp
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/glutSimple-OSG.dsw
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/license.txt
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/Makefile
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/README.txt
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/Data/Simple.ini
zip $DIST_FILE1 VTP/TerrainApps/glutSimple/Data/Elevation/README.txt

zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/*.h
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/mfcSimple.dsp
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/mfcSimple.dsw
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/mfcSimple.rc
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/Data/Simple.ini
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/Data/Elevation/README.txt
zip $DIST_FILE1 VTP/TerrainApps/mfcSimple/res/*

zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/app.cpp
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/sdlSimple.dsp
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/sdlSimple.dsw
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/license.txt
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/Makefile
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/Data/Simple.ini
zip $DIST_FILE1 VTP/TerrainApps/sdlSimple/Data/Elevation/README.txt

zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/*.h
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/license.txt
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/Makefile
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/VTBuilder.ini
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/VTBuilder.rc
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/VTBuilder.dsp
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/VTBuilder.dsw
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/VTBuilder.wdr
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/bitmaps/*
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/icons/*
zip $DIST_FILE1 VTP/TerrainApps/VTBuilder/cursors/*

zip $DIST_FILE1 VTP/TerrainApps/wxSimple/*.cpp
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/*.h
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/Makefile
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/README.txt
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/wxSimple.dsp
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/wxSimple-OSG.dsw
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/Data/Simple.ini
zip $DIST_FILE1 VTP/TerrainApps/wxSimple/Data/Elevation/README.txt


# Create the archive containing the Source Help
rm -f $DIST_FILE2

zip $DIST_FILE2 VTP/TerrainSDK/vtdata/Doc/html/*
zip $DIST_FILE2 VTP/TerrainSDK/vtlib/Doc/html/*

echo $DIST_FILE1 ready.
echo $DIST_FILE2 ready.
