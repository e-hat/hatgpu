compile_shader() {
  glslc "shaders/$1" -o "shaders/bin/$1.spv"
}

mkdir -p 'shaders/bin/bdpt'
mkdir -p 'shaders/bin/forward'
mkdir -p 'shaders/bin/aabb'
compile_shader 'forward/shader.vert'
compile_shader 'forward/shader.frag'
compile_shader 'bdpt/main.comp'
compile_shader 'aabb/shader.vert'
compile_shader 'aabb/shader.frag'
