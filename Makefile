BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= RelWithDebInfo
INSTALL_PREFIX ?= /usr/local

.PHONY: all configure build install test clean distclean

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) --parallel

install: build
	cmake --install $(BUILD_DIR) --prefix $(INSTALL_PREFIX)
	ldconfig

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	cmake --build $(BUILD_DIR) --target clean

distclean:
	rm -rf $(BUILD_DIR)
