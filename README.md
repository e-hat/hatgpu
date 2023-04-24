# hatgpu

This is going to be a GPU bidirectional path tracer written with Vulkan. 

It's originally branched off of [my first vulkan project](https://github.com/e-hat/efvk.git).

## Build
Clone with 
```bash
git clone --recurse-submodules https://github.com/e-hat/hatgpu.git
```
Build with (for Linux)
```bash
mkdir build && build
cmake .. # or `cmake -G Ninja ..`
make # or `ninja`
```
