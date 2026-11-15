# Depth snapshot GPU diagnostic

From PowerShell in the project root, run
`& Amped-Host/toolkit/tests/vulkan_depth_snapshot/run_gpu_test.ps1 -RunGPU`.
Omit `-RunGPU` to compile the executable and the generated diagnostic shader
without creating a Vulkan instance or using the GPU. The script uses the
workspace's existing MinGW, Vulkan and glslang dependencies.

The GPU test clears a one-pixel D32S8 source to 0.25, copies its depth aspect
into a sampled snapshot, and compares it against 0.5 using the project's
generated stage-1 shadow comparison. It then clears the source to 0.75 and
samples the unchanged snapshot, before copying the updated source and sampling
again. The host reads the results; a pass requires `1, 1, 0`.

This is a focused diagnostic, not an end-to-end game test. It uses an explicit
LOD compute shader derived from the generated fragment shader, its own copy
and barriers, and a clear as the depth producer. It does not exercise graphics
interpolation, implicit LOD, the backend's snapshot selection or lifecycle,
or its actual depth attachment draws. A device lacking sampled D32S8 skips
with exit code 77.
