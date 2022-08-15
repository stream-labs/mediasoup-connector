@echo on
set DEPOT_TOOLS=depot_tools

if exist %DEPOT_TOOLS%\ goto skip_depot_install

echo "Download depot tools"
set PATH=%PATH%;C:\Program Files\7-Zip\
set DEPOT_TOOLS_URL=https://storage.googleapis.com/chrome-infra/%DEPOT_TOOLS%.zip
if exist %DEPOT_TOOLS%.zip (curl -kLO %DEPOT_TOOLS_URL% -f --retry 5 -z %DEPOT_TOOLS%.zip) else (curl -kLO %DEPOT_TOOLS_URL% -f --retry 5 -C -)
7z x %DEPOT_TOOLS%.zip -aoa -odepot_tools

:skip_depot_install

set PATH=%PATH%;%CD%\%DEPOT_TOOLS%\
set CHECKOUT_DIR=webrtc-checkout
if exist %CHECKOUT_DIR%\ goto skip_webrtc_checkout 
echo "Check out webrtc"

mkdir %CHECKOUT_DIR%
cd %CHECKOUT_DIR%
call fetch --nohooks webrtc
cd ..

:skip_webrtc_checkout 
echo "Prepare settings"

cd %CHECKOUT_DIR%
set COMPILER_MODE=vs2019
set WEBRCT_BRACH=m94
set WEBRCT_BRACH_PATH=refs/remotes/branch-heads/4606

REM set WEBRCT_BRACH=m104
REM set WEBRCT_BRACH_PATH=refs/remotes/branch-heads/5112

if %COMPILER_MODE%==vs2022 goto set_2022
if %COMPILER_MODE%==vs2019 goto set_2019

:set_2022
echo "Settings for 2022"
set GYP_MSVS_VERSION=2022
set GYP_MSVS_OVERRIDE_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\
set vs2022_install=C:\Program Files\Microsoft Visual Studio\2022\Community\
goto to_sync_and_build

:set_2019
echo "Settings for 2019"
set GYP_MSVS_VERSION=2019
set GYP_MSVS_OVERRIDE_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\
set vs2019_install=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\
goto to_sync_and_build

:to_sync_and_build 
echo "Ready to sync"
set GYP_GENERATORS=msvs-ninja,ninja
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

call gclient sync

cd src

echo "Checkout specific brach " %WEBRCT_BRACH% " "%WEBRCT_BRACH_PATH%
set OUT_DIR=out\%WEBRCT_BRACH%

call git checkout -b %WEBRCT_BRACH% %WEBRCT_BRACH_PATH% 
@echo on

call gclient sync -D

echo "Prepare project"
call gn gen %OUT_DIR% --ide=%COMPILER_MODE% --args="is_debug=false is_component_build=false is_clang=false rtc_include_tests=true use_rtti=true rtc_build_examples=false use_custom_libcxx=false enable_iterator_debugging=false libcxx_is_shared=false rtc_build_tools=false use_lld=false treat_warnings_as_errors=false  use_custom_libcxx_for_host=false target_os=\"win\" target_cpu=\"x64\""
echo "Start building"
call ninja -j6 -C %OUT_DIR% 
echo "Finished bilding " %OUT_DIR% 
cd ..
cd ..

pause
echo "Copy files"

mkdir webrtc_install 
set sourcePath=.\%CHECKOUT_DIR%\src
set resultPath=.\webrtc_install
 
xcopy %sourcePath%\*.h %resultPath%\  /s /c /y /r

copy %CD%\%CHECKOUT_DIR%\src\%OUT_DIR%\obj\webrtc.lib %resultPath%\  

echo  "Build media soup client"
git clone https://github.com/versatica/libmediasoupclient.git --recursive
cd libmediasoupclient
git checkout 8b36a91520a0f6ea3ed506814410176a9fc71d62

mkdir build 
cd build 
cmake -G "Visual Studio 16 2019"  -A x64 ../ -DLIBWEBRTC_INCLUDE_PATH="%CD%\..\..\webrtc_install" -DLIBWEBRTC_BINARY_PATH="%CD%\..\..\webrtc_install"  -DCMAKE_INSTALL_PREFIX="%CD%\..\..\libmediasoupclient_install" 
cmake --build . --target install --config RelWithDebInfo