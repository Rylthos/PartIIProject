# Voxel Renderer

## Setup

```
git submodule update --init --recursive
```

Additional Dependencies:
```
glfw
ffmpeg
msquic
```

## Build Instructions

```
cmake -DCMAKE_BUILD_TYPE={Debug/Release} -B build -S .
cmake --build build
```

This will build build both the renderer and voxelizer

## Renderer

To run the renderer
```
./build/src/renderer/Renderer
```

## Voxelizer

The voxelizer supports obj, glTF, and vox.
For glTF and vox animations are supported with the `--anim` flag, with an additional `-f` to set
the number of frames
```
./build/src/voxelizer/Voxelizer
```

## Network Setup

To run the renderer as a split-renderer a openssl certificate is required.
This can be generated with:
```
openssl req -nodes -new -x509 -keyout res/server.key -out res/server.cert
```

Passing `--enable-client-side` or `--enable-server-side` with the `-p,-i` flags
will allow the renderer to communicate over a network
