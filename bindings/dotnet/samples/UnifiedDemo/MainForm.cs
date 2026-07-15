using System.Text;
using Lw.PPOCRSharp;

namespace Lw.PPOCR.Demo;

internal sealed partial class MainForm : Form
{
    private OcrEngine? engine;
    private ImageFrame? imageFrame;
    private OcrResult? currentResult;
    private bool configDirty = true;
    private bool busy;

    internal MainForm()
    {
        InitializeComponent();
        BindEvents();
        LoadDefaults();
    }

    internal async Task RunExperienceSmokeAsync(string backend)
    {
        backendBox.SelectedIndex = backend.ToLowerInvariant() switch
        {
            "directml" or "dml" or "directml-cpu" or "dml-cpu" => 1,
            "openvino" => 2,
            "tensorrt" or "trt" => 3,
            _ => 0
        };
        if (backend.Equals("directml-cpu", StringComparison.OrdinalIgnoreCase) ||
            backend.Equals("dml-cpu", StringComparison.OrdinalIgnoreCase))
        {
            useGpuCheck.Checked = false;
        }
        classifierCheck.Checked = true;
        if (await InitializeEngineAsync())
        {
            await RecognizeAsync();
        }
    }

    private void BindEvents()
    {
        initializeButton.Click += async (_, _) => await InitializeEngineAsync();
        destroyButton.Click += (_, _) => DestroyEngine("引擎已销毁");
        manifestBrowseButton.Click += (_, _) => BrowseManifest();
        runtimeBrowseButton.Click += (_, _) => BrowseRuntimeRoot();
        openImageButton.Click += (_, _) => BrowseImage();
        recognizeButton.Click += async (_, _) => await RecognizeAsync();
        copyButton.Click += (_, _) => CopyText();
        backendBox.SelectedIndexChanged += (_, _) => ApplyBackendDefaults();
        manifestText.TextChanged += (_, _) => MarkConfigurationChanged();
        runtimeRootText.TextChanged += (_, _) => MarkConfigurationChanged();
        deviceId.ValueChanged += (_, _) => MarkConfigurationChanged();
        useGpuCheck.CheckedChanged += (_, _) => ApplyDeviceSelection();
        classifierCheck.CheckedChanged += (_, _) => MarkConfigurationChanged();
        limitSide.ValueChanged += (_, _) => MarkConfigurationChanged();
        detThreshold.ValueChanged += (_, _) => MarkConfigurationChanged();
        boxThreshold.ValueChanged += (_, _) => MarkConfigurationChanged();
        unclipRatio.ValueChanged += (_, _) => MarkConfigurationChanged();
        recBatch.ValueChanged += (_, _) => MarkConfigurationChanged();
        recConcurrency.ValueChanged += (_, _) => MarkConfigurationChanged();
        clsBatch.ValueChanged += (_, _) => MarkConfigurationChanged();
        DragEnter += OnDragEnter;
        DragDrop += OnDragDrop;
        Shown += (_, _) =>
        {
            if (workspaceSplit.ClientSize.Width > 0)
            {
                workspaceSplit.SplitterDistance =
                    (int)(workspaceSplit.ClientSize.Width * 0.68);
            }
        };
        FormClosed += (_, _) =>
        {
            engine?.Dispose();
            imageFrame?.Dispose();
        };
    }

    private void LoadDefaults()
    {
        backendBox.Items.AddRange(new object[]
        {
            new BackendItem("OpenCV DNN (CPU)", OcrBackend.OpenCvDnn),
            new BackendItem("ONNX Runtime (CPU / DML GPU)", OcrBackend.DirectML),
            new BackendItem("OpenVINO (CPU)", OcrBackend.OpenVINO),
            new BackendItem("TensorRT (NVIDIA GPU)", OcrBackend.TensorRT)
        });
        runtimeRootText.Text = Path.Combine("runtimes", "win-x64");
        manifestText.Text = Path.Combine("models", "ppocrv6-tiny", "model.json");
        backendBox.SelectedIndex = 0;
        destroyButton.Enabled = false;
        copyButton.Enabled = false;
        string sampleImage = ResolveAppPath(
            Path.Combine("models", "ppocrv6-tiny", "sample.jpg"));
        if (File.Exists(sampleImage))
        {
            LoadImage(sampleImage);
            statusLabel.Text = "体验模型和图片已准备，点击初始化后即可识别";
        }
        else
        {
            statusLabel.Text = "请选择模型清单和图片，然后初始化引擎";
        }
    }

    private async Task<bool> InitializeEngineAsync()
    {
        if (busy)
        {
            return false;
        }
        string manifestPath = ResolveAppPath(manifestText.Text);
        string runtimeRoot = ResolveAppPath(runtimeRootText.Text);
        if (!File.Exists(manifestPath))
        {
            ShowError($"找不到模型清单：{manifestPath}\r\n\r\n" +
                "相对路径以程序所在目录为基准。", "模型未准备");
            return false;
        }
        if (!Directory.Exists(runtimeRoot))
        {
            ShowError($"找不到 Runtime 根目录：{runtimeRoot}\r\n\r\n" +
                "请选择包含各后端目录的 runtimes/win-x64。", "运行库未准备");
            return false;
        }

        OcrOptions options;
        try
        {
            options = CreateOptions();
        }
        catch (Exception exception)
        {
            ShowError(exception.Message, "配置错误");
            return false;
        }

        SetBusy(true, "正在加载模型和运行库...");
        OcrEngine? replacement = null;
        try
        {
            replacement = await Task.Run(() => new OcrEngine(options));
            engine?.Dispose();
            engine = replacement;
            replacement = null;
            configDirty = false;
            OcrCapabilities capabilities = engine.Capabilities;
            statusLabel.Text = $"初始化成功：{capabilities.BackendName} | " +
                $"运行设备={CurrentDeviceDescription()} | " +
                $"CPU={YesNo(capabilities.SupportsCpu)} GPU={YesNo(capabilities.SupportsGpu)} " +
                $"FP16={YesNo(capabilities.SupportsFp16)} CLS={YesNo(capabilities.SupportsClassifier)}";
            return true;
        }
        catch (Exception exception)
        {
            ShowError(exception.Message, "初始化失败");
            return false;
        }
        finally
        {
            replacement?.Dispose();
            SetBusy(false);
        }
    }

    private async Task RecognizeAsync()
    {
        if (busy)
        {
            return;
        }
        if (imageFrame is null)
        {
            BrowseImage();
            if (imageFrame is null)
            {
                return;
            }
        }
        if (engine is null || configDirty)
        {
            if (!await InitializeEngineAsync())
            {
                return;
            }
        }

        OcrEngine activeEngine = engine!;
        ImageFrame activeImage = imageFrame;
        SetBusy(true, "正在识别...");
        try
        {
            OcrResult result = await Task.Run(() => activeEngine.Run(
                activeImage.Pixels,
                activeImage.Width,
                activeImage.Height,
                activeImage.Stride,
                activeImage.PixelFormat));
            currentResult = result;
            ShowResult(result);
            statusLabel.Text = $"识别完成，共 {result.Regions.Count} 个文本区域";
        }
        catch (Exception exception)
        {
            ShowError(exception.Message, "识别失败");
        }
        finally
        {
            SetBusy(false);
        }
    }

    private OcrOptions CreateOptions()
    {
        if (backendBox.SelectedItem is not BackendItem selected)
        {
            throw new InvalidOperationException("请选择推理后端。");
        }
        return new OcrOptions
        {
            Backend = selected.Backend,
            DeviceId = (int)deviceId.Value,
            RuntimeRoot = ResolveAppPath(runtimeRootText.Text),
            ModelManifest = ResolveAppPath(manifestText.Text),
            BackendOptionsJson = selected.Backend switch
            {
                OcrBackend.OpenVINO => "{\"device\":\"CPU\"}",
                OcrBackend.DirectML =>
                    $"{{\"use_gpu\":{(useGpuCheck.Checked ? 1 : 0)}}}",
                _ => null
            },
            EnableClassifier = classifierCheck.Checked,
            LimitSideLength = (int)limitSide.Value,
            DetectionThreshold = (float)detThreshold.Value,
            DetectionBoxThreshold = (float)boxThreshold.Value,
            DetectionUnclipRatio = (float)unclipRatio.Value,
            ClassifierBatchSize = (int)clsBatch.Value,
            RecognitionBatchSize = (int)recBatch.Value,
            RecognitionConcurrency = (int)recConcurrency.Value
        };
    }

    private void ShowResult(OcrResult result)
    {
        resultGrid.Rows.Clear();
        var text = new StringBuilder();
        for (int index = 0; index < result.Regions.Count; index++)
        {
            OcrTextRegion region = result.Regions[index];
            resultGrid.Rows.Add(index + 1, region.Text, region.Score.ToString("P1"));
            text.AppendLine(region.Text);
        }
        plainText.Text = text.ToString();
        imageView.SetContent(imageFrame?.Bitmap, result.Regions);
        copyButton.Enabled = result.Regions.Count > 0;
        timingLabel.Text =
            $"DET {result.Detector.TotalMilliseconds:F1} ms  |  " +
            $"CLS {result.Classifier.TotalMilliseconds:F1} ms  |  " +
            $"REC {result.Recognizer.TotalMilliseconds:F1} ms  |  " +
            $"总计 {result.Pipeline.TotalMilliseconds:F1} ms";
    }

    private void BrowseManifest()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "选择统一模型清单",
            Filter = "PPOCR 模型清单 (model.json)|model.json|JSON 文件 (*.json)|*.json|所有文件 (*.*)|*.*",
            CheckFileExists = true
        };
        string currentManifest = ResolveAppPath(manifestText.Text);
        if (File.Exists(currentManifest))
        {
            dialog.InitialDirectory = Path.GetDirectoryName(currentManifest);
        }
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            manifestText.Text = ToAppRelativePath(dialog.FileName);
        }
    }

    private void BrowseRuntimeRoot()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "选择包含 opencv、directml、openvino、tensorrt 的 Runtime 根目录",
            UseDescriptionForTitle = true,
            SelectedPath = Directory.Exists(ResolveAppPath(runtimeRootText.Text))
                ? ResolveAppPath(runtimeRootText.Text)
                : AppContext.BaseDirectory
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            runtimeRootText.Text = ToAppRelativePath(dialog.SelectedPath);
        }
    }

    private void BrowseImage()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "选择测试图片",
            Filter = "图片文件|*.jpg;*.jpeg;*.png;*.bmp|所有文件 (*.*)|*.*",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            LoadImage(dialog.FileName);
        }
    }

    private void LoadImage(string path)
    {
        try
        {
            ImageFrame replacement = ImageFrame.Load(path);
            imageFrame?.Dispose();
            imageFrame = replacement;
            currentResult = null;
            resultGrid.Rows.Clear();
            plainText.Clear();
            timingLabel.Text = string.Empty;
            copyButton.Enabled = false;
            imageView.SetContent(imageFrame.Bitmap, null);
            statusLabel.Text = $"已加载：{Path.GetFileName(path)} ({imageFrame.Width} x {imageFrame.Height})";
        }
        catch (Exception exception)
        {
            ShowError(exception.Message, "图片加载失败");
        }
    }

    private void ApplyBackendDefaults()
    {
        if (backendBox.SelectedItem is not BackendItem selected)
        {
            return;
        }
        switch (selected.Backend)
        {
            case OcrBackend.DirectML:
                useGpuCheck.Enabled = true;
                useGpuCheck.Checked = true;
                recBatch.Value = 4;
                recConcurrency.Value = 4;
                break;
            case OcrBackend.OpenVINO:
                useGpuCheck.Checked = false;
                useGpuCheck.Enabled = false;
                recBatch.Value = 1;
                recConcurrency.Value = 8;
                break;
            case OcrBackend.TensorRT:
                useGpuCheck.Checked = true;
                useGpuCheck.Enabled = false;
                recBatch.Value = 4;
                recConcurrency.Value = 4;
                break;
            default:
                useGpuCheck.Checked = false;
                useGpuCheck.Enabled = false;
                recBatch.Value = 1;
                recConcurrency.Value = 2;
                break;
        }
        deviceId.Enabled = useGpuCheck.Checked &&
            selected.Backend is OcrBackend.DirectML or OcrBackend.TensorRT;
        MarkConfigurationChanged();
    }

    private void ApplyDeviceSelection()
    {
        if (backendBox.SelectedItem is not BackendItem selected)
        {
            return;
        }
        deviceId.Enabled = useGpuCheck.Checked &&
            selected.Backend is OcrBackend.DirectML or OcrBackend.TensorRT;
        if (selected.Backend == OcrBackend.DirectML)
        {
            recBatch.Value = useGpuCheck.Checked ? 4 : 1;
            recConcurrency.Value = useGpuCheck.Checked ? 4 : 2;
        }
        if (!useGpuCheck.Checked)
        {
            deviceId.Value = 0;
        }
        MarkConfigurationChanged();
    }

    private void MarkConfigurationChanged()
    {
        configDirty = true;
        if (engine is not null && !busy)
        {
            statusLabel.Text = "配置已修改，下次识别前会重新初始化";
        }
    }

    private void DestroyEngine(string message)
    {
        if (busy)
        {
            return;
        }
        engine?.Dispose();
        engine = null;
        configDirty = true;
        destroyButton.Enabled = false;
        statusLabel.Text = message;
    }

    private void SetBusy(bool value, string? message = null)
    {
        busy = value;
        initializeButton.Enabled = !value;
        recognizeButton.Enabled = !value;
        openImageButton.Enabled = !value;
        destroyButton.Enabled = !value && engine is not null;
        backendBox.Enabled = !value;
        manifestText.Enabled = !value;
        runtimeRootText.Enabled = !value;
        UseWaitCursor = value;
        if (!string.IsNullOrEmpty(message))
        {
            statusLabel.Text = message;
        }
    }

    private void CopyText()
    {
        if (!string.IsNullOrEmpty(plainText.Text))
        {
            Clipboard.SetText(plainText.Text.TrimEnd());
            statusLabel.Text = "识别文本已复制到剪贴板";
        }
    }

    private void OnDragEnter(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true &&
            e.Data.GetData(DataFormats.FileDrop) is string[] files &&
            files.Length == 1 && IsSupportedImage(files[0]))
        {
            e.Effect = DragDropEffects.Copy;
        }
    }

    private void OnDragDrop(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetData(DataFormats.FileDrop) is string[] files && files.Length > 0)
        {
            LoadImage(files[0]);
        }
    }

    private void ShowError(string message, string title)
    {
        statusLabel.Text = message;
        MessageBox.Show(this, message, title, MessageBoxButtons.OK, MessageBoxIcon.Error);
    }

    private static bool IsSupportedImage(string path)
    {
        string extension = Path.GetExtension(path);
        return extension.Equals(".jpg", StringComparison.OrdinalIgnoreCase) ||
            extension.Equals(".jpeg", StringComparison.OrdinalIgnoreCase) ||
            extension.Equals(".png", StringComparison.OrdinalIgnoreCase) ||
            extension.Equals(".bmp", StringComparison.OrdinalIgnoreCase);
    }

    private static string YesNo(bool value) => value ? "是" : "否";

    private string CurrentDeviceDescription()
    {
        if (backendBox.SelectedItem is BackendItem selected &&
            selected.Backend is OcrBackend.DirectML or OcrBackend.TensorRT &&
            useGpuCheck.Checked)
        {
            return $"GPU {deviceId.Value}";
        }
        return "CPU";
    }

    private static string ResolveAppPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return string.Empty;
        }
        string expanded = Environment.ExpandEnvironmentVariables(path.Trim());
        return Path.GetFullPath(Path.IsPathRooted(expanded)
            ? expanded
            : Path.Combine(AppContext.BaseDirectory, expanded));
    }

    private static string ToAppRelativePath(string path)
    {
        return Path.GetRelativePath(
            AppContext.BaseDirectory,
            Path.GetFullPath(path));
    }

    private sealed record BackendItem(string Name, OcrBackend Backend)
    {
        public override string ToString() => Name;
    }
}
