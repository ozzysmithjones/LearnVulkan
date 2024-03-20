set p=%cd%
%p%\glslc.exe shaders\shader.vert -o vert.spv
%p%\glslc.exe shaders\shader.frag -o frag.spv
pause