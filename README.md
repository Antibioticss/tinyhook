# tinyhook

> A minimalist hook framework for macOS/iOS, built for speed and size

## Features

 - inline hook
 - symbol interposing
 - symbol resolving
 - objc runtime hook

## Building

Build tinyhook yourself or download pre-built binaries from [Releases](https://github.com/Antibioticss/tinyhook/releases)

Nightly builds are available from [Actions](https://github.com/Antibioticss/tinyhook/actions)

### Build with `make`

```bash
make
```

Available targets:
 - `static` (default) build static library
 - `shared` build shared library
 - `all` build both static and shared libraries
 - `test` run tinyhook tests

Available variables:
 - `ARCH` the arch to build: `arm64`, `arm64e`, `x86_64`
 - `TARGET` targeting os: `macosx`(default), `iphoneos`
 - `MIN_OSVER` minimum os version requirement
 - `DEBUG` generate debug infomation
 - `COMPACT` no error log output (not recommended!)

For example, building shared library for iOS 18.0+ `arm64e` binary with `DEBUG` enabled

```bash
make shared ARCH=arm64e TARGET=iphoneos MIN_OSVER=18.0 DEBUG=1
```

### Use `build.sh`

`build.sh` can be used to build universal FAT binaries (i.e. a single binary with multiple arches)

```
arguments:
  -a <arch>     add an arch to build
  -t <target>   specify target system, macosx(default) or iphoneos
  -v <version>  specify minimum system version
  -c            build compact version
```

For example, the below commands are used to build binaries for releasing

```bash
./build.sh -t macosx -v 10.15
./build.sh -t iphoneos -v 12.0
```

Output binaries will be in `build/universal`

## Documentation

Served on GitHub Pages: https://antibioticss.github.io/tinyhook/

## Example

Source code of the examples: [tinyhook/test](https://github.com/Antibioticss/tinyhook/tree/main/test)

## References

Thanks to these projects for their inspiring idea and code!

- <https://github.com/rodionovd/rd_route>
- <https://github.com/GiveMeZeny/fde64>
- <https://github.com/facebook/fishhook>
