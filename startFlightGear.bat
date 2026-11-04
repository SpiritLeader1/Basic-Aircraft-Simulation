@echo off
fgfs --fdm=external ^
    --native-fdm=socket,in,60,,5500,udp ^
    --aircraft=c172p ^
    --lat=39.6 ^
    --lon=32.4 ^
    --altitude=3281 ^
    --heading=90 ^
    --timeofday=noon ^
    --enable-hud ^
    --disable-hud-3d