compile_shader() {
  glslc "shaders/$1" -o "shaders/bin/$1.spv"
}

mkdir -p 'shaders/bin/bdpt'
mkdir -p 'shaders/bin/forward'
compile_shader 'forward/shader.vert'
compile_shader 'forward/shader.frag'
compile_shader 'bdpt/main.comp'
