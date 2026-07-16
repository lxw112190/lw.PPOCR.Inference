# WinForms 快速体验

1. 双击 `lw.PPOCR.Demo.exe`。程序基于 .NET Framework 4.7.2，可直接使用 Win10/Win11 的 .NET Framework 4.x 运行环境。
2. 程序会自动选择本拆分包对应的推理后端，并加载自带的体验图片。
3. 点击“初始化”，等待模型加载完成。
4. 点击“识别”查看文字区域、识别结果和各阶段耗时。
5. 在图片上按住鼠标左键拖出文字区域，松开后 Demo 会跳过检测，直接识别框选内容并显示本次耗时。

也可以将自己的 JPG、PNG 或 BMP 图片直接拖入窗口。

默认模型位于 `models/ppocrv6-tiny`，Runtime 位于 `runtimes/win-x64`。请保持这些目录与 EXE 的相对位置不变。

DirectML 和 TensorRT 的 GPU 可用性取决于本机显卡、驱动以及相关运行时要求；OpenCV 和 OpenVINO 包默认使用 CPU。

如果目标系统经过裁剪且没有兼容的 .NET Framework，请先安装 .NET Framework 4.7.2 或更高版本。

需要通过浏览器或其他程序调用时，可双击 `run-http-service.cmd` 启动本机 HTTP API 和测试网页；也可使用 `install-service.cmd` 安装为 Windows 服务。
