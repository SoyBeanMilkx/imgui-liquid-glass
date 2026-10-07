# ImGui Liquid Glass

基于 [Dear ImGui](https://github.com/ocornut/imgui) 的 Android 液体玻璃控件与悬浮界面项目。通过实时背景采样、高斯模糊、边缘折射、色散和高光，让窗口与交互控件融入宿主画

项目包含可复用的控件层、Vulkan / OpenGL ES 渲染后端，以及一套展示导航、输入、滚动和窗口交互的示例界面

> 仓库不包含 `hridhi`、`toucher` 的实现，也不包含 `.a`、`.so` 或 `.kpm`。完整示例需要使用者提供对应依赖，或替换平台适配层；仅克隆仓库还不能直接编译完整示例

## 演示

![sjz](art/sjz.gif)

![wzry](art/wzry.gif)

## 功能

- **液体玻璃**：实时背景捕获、高斯模糊、折射、色散、染色、内外阴影和边缘高光；支持圆角矩形、胶囊与圆形
- **基础控件**：`Text`、`Icon`、`Button`、`Switch`、`SwitchRow`、`Slider`、`TextField`、`TabBar` 和 `ResizeHandle`
- **容器与导航**：`GlassWindow`、`LayoutViewport`、`ScrollView`、`SwipePager`、`GlassNavigationLayout` 和 `NavigationRail`
- **移动端交互**：触摸方向判定、滚动惯性与回弹、横向切页、输入框焦点和 Android 输入法桥接
- **动画与布局**：弹簧动画、按压反馈、页面过渡，以及窗口和内部内容同步的位移与缩放
- **双渲染后端**：Vulkan 与 OpenGL ES；玻璃能力不可用时，控件可回退到普通绘制

## 使用与接入

### 复用控件层

已有 ImGui 宿主时，可以复用 `src/ui/widget/`，接入自己的输入与渲染流程。控件业务代码从 `ui/widget/Widget.hpp` 开始；字体、Shader 的生成配置位于各自的 `CMakeLists.txt` 中

宿主需要维护一个跨帧存活的 `widget::Context`，并提供以下流程：

1. 更新 ImGui 输入，以及 `FrameInfo` 中的显示尺寸、时间步长、密度、帧序号和指针事件
2. 在 `ImGui::NewFrame()` 前调用 `context.beginFrame()`；容器存在呈现变换时，同时传入对应的输入空间
3. 在 ImGui 帧中绘制控件，随后调用 `context.endFrame()`
4. 在 `ImGui::Render()` 后调用 `context.prepareDrawData()`，统一应用绘制变换
5. 渲染后端按玻璃请求捕获背景、执行模糊并合成效果，同时向 Context 提供真实的 `EffectCapabilities`

完整帧流程可参考 [`OverlayFrameRuntime.cpp`](src/ui/renderer/OverlayFrameRuntime.cpp)，玻璃接入可参考 [`OpenGLRenderer.cpp`](src/ui/renderer/opengl/OpenGLRenderer.cpp) 和 [`SwapchainRenderer.cpp`](src/ui/renderer/vulkan/SwapchainRenderer.cpp)。只接普通 ImGui 渲染器时，可以使用基础控件和玻璃效果不可用时的回退绘制；实时折射与模糊还需要玻璃后端

当前根目录 CMake 构建的是完整 Android 示例 `libglass_ui.so`。只复用控件时，需要为选取的源码建立自己的构建目标，并接入所需的字体、Shader 和后端，不必保留示例的 Hook 与触摸捕获实现

### 运行完整示例

完整示例通过宿主的图形帧绘制界面，加载入口位于 [`src/main.cpp`](src/main.cpp)。当前入口显式选择 `GraphicsBackend::Vulkan`；OpenGL ES 宿主应改为 `GraphicsBackend::OpenGLES` 后重新编译

使用者需要提供动态库加载方式，并让渲染、触摸和输入法接入适合自己的宿主环境。仓库包含示例接入代码，不包含通用加载器

### 替换未公开的后端

`hridhi` 与 `toucher` 是当前示例采用的后端，使用者可以提供兼容实现，也可以用自己的实现修改适配层

| 接入能力 | 需要提供的行为 | 主要适配位置 |
| --- | --- | --- |
| 图形 API / Hook | 接入宿主绘制时机、获取所需图形函数，并保持宿主调用与图形资源生命周期正确 | `src/core/graphics/` |
| 动态库加载监听 | 在目标动态库出现后完成相应接入；当前实现使用 `hridhi` 监听加载过程 | `src/core/hooks/ElfLoadMonitor.cpp` |
| 触摸输入 | 提供按下、移动、抬起和取消事件，处理屏幕坐标与旋转，并按 UI 区域分配触摸归属 | `src/ui/input/touch/InputBridge.*` |
| Android 编辑与应用窗口触摸 | 提供可用的窗口和 Java 环境，完成输入法、编辑会话与触摸桥接 | `src/ui/input/AndroidIme.*`、`src/ui/input/touch/TouchBridge.*`、`src/ui/input/java/` |

兼容实现可以按 `include/hridhi/hridhi.h` 与 `include/toucher/toucher.h` 提供对应 C 接口。采用其他接口时，应修改上表中的适配代码，并同步移除或替换根目录 CMake 中的 `hridhi`、`toucher` 静态库导入与链接配置

接入自己的应用帧与触摸回调时，也可以绕过示例的 Hook 和捕获流程。上层控件仍通过 Context、指针事件和输入会话工作；替换后端不需要重写每个控件

## 构建

运行目标是 Android。Windows、macOS 和 Linux 可作为交叉编译主机；仓库没有提供桌面应用后端

需要准备：

- Android NDK、CMake 和 Ninja；源码使用 C++17
- Android SDK 的 Platform 与 Build Tools，用于生成 Java 输入桥的 DEX
- 提供 `javac` 且支持 `--release 8` 的 JDK，并满足所用 Android Build Tools 的要求
- 主机可执行的 `glslc`，用于编译 Vulkan Shader
- 与目标 ABI、STL 和头文件匹配的依赖库

当前 CMake 从以下位置导入静态库。这些文件不在仓库中，需要自行构建或提供：

| 路径 | 内容 |
| --- | --- |
| `include/imgui/bin/libimgui_core.a` | Dear ImGui 核心 |
| `include/imgui/bin/libimgui_vulkan.a` | Dear ImGui Vulkan 后端 |
| `include/imgui/bin/libimgui_opengl3.a` | Dear ImGui OpenGL 后端 |
| `include/hridhi/bin/libhridhi.a` | 兼容当前接口的 Hook 后端 |
| `include/toucher/bin/libtoucher.a` | 兼容当前接口的触摸后端 |

仓库的 Dear ImGui 头文件标记为 `1.93.0 WIP`。编译 ImGui 库时，需要使用匹配的源码版本与 `imconfig.h`，不能仅凭相近的版本号混用头文件和二进制。核心库包含 `imgui.cpp`、`imgui_draw.cpp`、`imgui_tables.cpp` 与 `imgui_widgets.cpp`，两个后端分别来自 `imgui_impl_vulkan.cpp` 和 `imgui_impl_opengl3.cpp`

以下是 Windows PowerShell 的构建示例，路径按自己的环境修改。示例目标为 `arm64-v8a`、Android API 24，STL 默认使用 `c++_static`：

```powershell
$ndkRoot = "D:/Android_NDK/android-ndk-r29"
$sdkRoot = "C:/Users/your-name/AppData/Local/Android/Sdk"

cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=$ndkRoot/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-24 `
  "-DGLASS_UI_GLSLC=$ndkRoot/shader-tools/windows-x86_64/glslc.exe" `
  "-DGLASS_UI_ANDROID_SDK=$sdkRoot"

cmake --build build --target glass_ui --parallel
```

产物为 `build/libglass_ui.so`。Shader、字体和 Java 输入桥会在构建过程中生成并嵌入动态库

macOS / Linux 使用相同的 CMake 参数，替换 NDK、SDK 和主机 `glslc` 路径即可。CMake 会尝试查找 NDK 中的 `glslc`，SDK 可通过 `GLASS_UI_ANDROID_SDK`、`ANDROID_SDK_ROOT` 或 `ANDROID_HOME` 指定

CLion 中应通过 `CMAKE_TOOLCHAIN_FILE` 使用 Android 工具链。如果手动填写 C/C++ 编译器，路径应指向 NDK 的编译器可执行文件，而不是 NDK 根目录

## 控件示例

下面的函数在已经开始的 ImGui / Widget 帧内调用，展示玻璃窗口、输入框、滑块和按钮的组合：

```cpp
#include "ui/widget/Widget.hpp"

#include <string>

namespace w = glass_ui::widget;

void DrawDemo(w::Context &context) {
  static std::string name;
  static float level = 0.5f;

  ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f), ImGuiCond_FirstUseEver);

  w::GlassWindowOptions window;
  window.radii = w::CornerRadii::all(28.0f);
  window.flags = ImGuiWindowFlags_NoTitleBar;

  if (w::BeginGlassWindow(context, "##demo", nullptr, window)) {
    w::Text(context, "Liquid Glass");

    w::TextFieldOptions input;
    input.hint = "Display name";
    w::TextField(context, "##name", &name, input);

    w::Slider(context, "##level", &level, 0.0f, 1.0f);

    if (w::Button(context, "Reset"))
      level = 0.5f;
  }
  w::EndGlassWindow(context);
}
```

控件的值由调用者持有；使用稳定且唯一的 ID，让 Context 保存跨帧的交互与动画状态。所有 `Begin` / `End` 容器调用都应配对，即使 `Begin` 返回 `false`。Android 软键盘还需要宿主接好输入会话与 IME 桥，单独调用 `TextField` 不会自动完成平台接入

## 样式与动画

- `context.theme()` 设置字体、字号、颜色和默认尺寸；单个控件的 Options 可以覆盖默认值
- `GlassStyle` 调整折射、厚度、色散、染色与光照；`GlassBackdropStyle` 设置背景模糊和采样质量
- 控件自身的按压、聚焦和选中反馈由通用动画机制管理。示例 UI 的窗口晃动、展开和切页由编排层驱动，可以单独替换或省略
- 自建宿主可在 `FrameInfo` 中设置 `reduceMotion = true`，让内置动画按目标状态直接更新；`TextFieldOptions::focusAnimation = false` 可单独关闭输入框的聚焦缩放
- 组合容器通过公共绘制变换同步移动内部内容，输入通过对应的输入空间逆映射。自定义前景绘制可使用 `context.foreground()`

默认内置字体为裁剪后的 SF Pro Text Regular / Semibold，只覆盖拉丁字符。中文界面需要自行注册包含所需字形的字体，可参考 `src/ui/widget/foundation/typography/Font.hpp`
