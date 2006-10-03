//
// Use this file to configure the vtdata library.
//
// Set each option to either 0 or 1 as desired.
//

#ifndef CONFIG_VTDATA_H
#define CONFIG_VTDATA_H

// Use the QuikGrid library, which adds support for operations such as
// generating contour lines.
//
// The QuikGrid library is LGPL and available from:
//		http://www.perspectiveedge.com/
//
#define SUPPORT_QUIKGRID	1

// Use the NetCDF library, which adds support for the "CDF" file format,
// which is commonly used for bathymetry.
//
// The NetCDF library is available from:
//		http://www.unidata.ucar.edu/packages/netcdf/
//
#define SUPPORT_NETCDF	1

// Use the Curl library, which adds cross-platform support for HTTP
// operations such as reading from WFS (Web Feature Servers).
//
// The Curl library is available from:
//		http://curl.haxx.se/libcurl/
//
#define SUPPORT_HTTP	1

// Use the bzip2 library, which adds support for reading from .bz2 files
//
// The library is free (BSD-style license) and available from:
// http://www.bzip.org/
//
#define SUPPORT_BZIP2	1

// Set to 1 if your C++ compiler supports wide strings (std::wstring)
//
// Apparently, there is some environment on the Macintosh without this.
// If 0, then international characters in strings may not (will not)
// be handled correctly.
//
#define SUPPORT_WSTRING	1

#endif // CONFIG_VTDATA_H

