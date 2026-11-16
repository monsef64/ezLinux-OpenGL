# Make sure this project is built when the Editor is built
# Note: the target only exists when the DXC based shader compiler toolchain is available
# (see CMakeLists.txt), so only register the dependency in that case.
if(TARGET ShaderCompilerOpenGL)
  ez_add_as_runtime_dependency(ShaderCompilerOpenGL)

  ez_add_dependency("ShaderCompiler" "ShaderCompilerOpenGL")
endif()
