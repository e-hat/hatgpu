compile_shader() {
  glslc "shaders/$1" -o "shaders/bin/$1.spv"
}

mkdir -p 'shaders/bin/bdpt'
mkdir -p 'shaders/bin/forward'
mkdir -p 'shaders/bin/layers'
compile_shader 'forward/shader.vert'
compile_shader 'forward/shader.frag'
compile_shader 'bdpt/main.comp'
compile_shader 'layers/aabb.vert'
