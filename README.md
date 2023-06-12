# hatgpu

This is _**going to be**_ a GPU bidirectional path tracer written with Vulkan. 

It's originally branched off of [my first vulkan project](https://github.com/e-hat/efvk.git).

## Build
Clone it:
```bash
git clone --recurse-submodules https://github.com/e-hat/hatgpu.git
```
Build it (for Linux):
```bash
mkdir build && cd build
cmake .. # or `cmake -G Ninja ..`
make # or `ninja`
```
Run it:
```bash
./hatgpu
```
Read the ImGui window for controller instructions.
