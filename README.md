# Vulkan Sync Graph
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/azhirnovgithub)<br/>


## Suported Platforms
* Windows 10 (with MSVC 2019)


## Building
Generate project with CMake and build.<br/>
Required C++17 standard support.


## How to use

### Explicit layer
Add DWORD value `<binary_dir>\VK_LAYER_AZ_sync_analyzer.json` into `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\ExplicitLayers`.<br/>
Add instance layer `VK_LAYER_AZ_sync_analyzer` when creating instance.<br/>
Press `F11` to capture some frames.<br/>

### Implicit layer
Add DWORD value `<binary_dir>\VK_LAYER_AZ_sync_analyzer.json` into `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\ImplicitLayers`.<br/>
Add environment variable `ENABLE_VK_LAYER_AZ_sync_analyzer_1` with value 1.<br/>
Run vulkan application (game) and press `F11` to capture some frames.<br/>


## Roadmap

Visualization:
- [x] Visualize CPU-GPU synchronizations
- [x] Visualize synchronizations with VkSemaphore
- [ ] Visualize pipeline barriers
- [ ] Measure CPU time
- [ ] Measure GPU time


## Examples

Sample<br/>
![image](images/sample.png)
<br/>
Rage 2<br/>
![image](images/rage2.png)
<br/>
Wolfenshtain II<br/>
![image](images/wolfenshtein2.png)
