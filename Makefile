.PHONY: deps

version = 7.3.492.25

export PATH := $(PATH):$(shell pwd)/depot_tools

UNAME_S := $(shell uname -s)


depot_tools:
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

v8: depot_tools
	fetch v8
	cd v8; gclient sync -j 4 -r $(version) -v --no-history; cd ..


d8: v8 v8/out.gn/d8

v8/out.gn/d8:
	cd v8; gn gen --args="use_sysroot=false is_debug=false is_component_build=true v8_static_library=false v8_use_external_startup_data=false v8_enable_i18n_support=true is_clang=false use_custom_libcxx=false treat_warnings_as_errors=false" out.gn; ninja -j4 -C out.gn d8; cd ..

deps: v8
ifeq ($(UNAME_S),Linux)
	./v8/build/install-build-deps.sh
endif

