# Clone chromium depot_tools and add to path
mkdir depot_tools
cd depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PWD:$PATH
cd ..

# Download and compile webrtc, mac_min_system_version=10.13.0
mkdir webrtc-checkout
cd webrtc-checkout
fetch --nohooks webrtc
gclient sync
cd src
git checkout -b m94 refs/remotes/branch-heads/4606
gclient sync
gn gen out/m94 --args='target_os="mac" target_cpu="x64" mac_deployment_target="10.13.0" mac_min_system_version="10.13.0" mac_sdk_min="10.13" is_debug=false is_component_build=false is_clang=true rtc_include_tests=false use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false' --ide=xcode
ninja -C out/m94