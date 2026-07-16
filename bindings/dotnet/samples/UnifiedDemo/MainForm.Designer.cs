#nullable enable

namespace Lw.PPOCR.Demo;

partial class MainForm
{
    private System.ComponentModel.IContainer? components;

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            components?.Dispose();
        }
        base.Dispose(disposing);
    }

    private void InitializeComponent()
    {
        components = new System.ComponentModel.Container();
        rootLayout = new TableLayoutPanel();
        configurationPanel = new Panel();
        configurationLayout = new TableLayoutPanel();
        primaryLayout = new TableLayoutPanel();
        backendLabel = new Label();
        backendBox = new ComboBox();
        manifestLabel = new Label();
        manifestText = new TextBox();
        manifestBrowseButton = new Button();
        runtimeLabel = new Label();
        runtimeRootText = new TextBox();
        runtimeBrowseButton = new Button();
        initializeButton = new Button();
        destroyButton = new Button();
        settingsPanel = new FlowLayoutPanel();
        limitSideLabel = new Label();
        limitSide = new NumericUpDown();
        detThresholdLabel = new Label();
        detThreshold = new NumericUpDown();
        boxThresholdLabel = new Label();
        boxThreshold = new NumericUpDown();
        unclipRatioLabel = new Label();
        unclipRatio = new NumericUpDown();
        recBatchLabel = new Label();
        recBatch = new NumericUpDown();
        recConcurrencyLabel = new Label();
        recConcurrency = new NumericUpDown();
        clsBatchLabel = new Label();
        clsBatch = new NumericUpDown();
        useGpuCheck = new CheckBox();
        deviceIdLabel = new Label();
        deviceId = new NumericUpDown();
        classifierCheck = new CheckBox();
        workspacePanel = new Panel();
        workspaceSplit = new SplitContainer();
        imagePanel = new TableLayoutPanel();
        imageToolbar = new FlowLayoutPanel();
        openImageButton = new Button();
        recognizeButton = new Button();
        imageHintLabel = new Label();
        imageView = new OcrImageView();
        resultPanel = new TableLayoutPanel();
        resultToolbar = new FlowLayoutPanel();
        resultTitleLabel = new Label();
        copyButton = new Button();
        resultTabs = new TabControl();
        structuredTab = new TabPage();
        resultGrid = new DataGridView();
        indexColumn = new DataGridViewTextBoxColumn();
        textColumn = new DataGridViewTextBoxColumn();
        scoreColumn = new DataGridViewTextBoxColumn();
        textTab = new TabPage();
        plainText = new TextBox();
        statusBar = new StatusStrip();
        statusLabel = new ToolStripStatusLabel();
        timingLabel = new ToolStripStatusLabel();
        browseToolTip = new ToolTip(components);
        rootLayout.SuspendLayout();
        configurationPanel.SuspendLayout();
        configurationLayout.SuspendLayout();
        primaryLayout.SuspendLayout();
        settingsPanel.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)limitSide).BeginInit();
        ((System.ComponentModel.ISupportInitialize)detThreshold).BeginInit();
        ((System.ComponentModel.ISupportInitialize)boxThreshold).BeginInit();
        ((System.ComponentModel.ISupportInitialize)unclipRatio).BeginInit();
        ((System.ComponentModel.ISupportInitialize)recBatch).BeginInit();
        ((System.ComponentModel.ISupportInitialize)recConcurrency).BeginInit();
        ((System.ComponentModel.ISupportInitialize)clsBatch).BeginInit();
        ((System.ComponentModel.ISupportInitialize)deviceId).BeginInit();
        workspacePanel.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)workspaceSplit).BeginInit();
        workspaceSplit.Panel1.SuspendLayout();
        workspaceSplit.Panel2.SuspendLayout();
        workspaceSplit.SuspendLayout();
        imagePanel.SuspendLayout();
        imageToolbar.SuspendLayout();
        resultPanel.SuspendLayout();
        resultToolbar.SuspendLayout();
        resultTabs.SuspendLayout();
        structuredTab.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)resultGrid).BeginInit();
        textTab.SuspendLayout();
        statusBar.SuspendLayout();
        SuspendLayout();

        rootLayout.ColumnCount = 1;
        rootLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        rootLayout.Controls.Add(configurationPanel, 0, 0);
        rootLayout.Controls.Add(workspacePanel, 0, 1);
        rootLayout.Controls.Add(statusBar, 0, 2);
        rootLayout.Dock = DockStyle.Fill;
        rootLayout.RowCount = 3;
        rootLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 126F));
        rootLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
        rootLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 28F));

        configurationPanel.BackColor = Color.White;
        configurationPanel.Controls.Add(configurationLayout);
        configurationPanel.Dock = DockStyle.Fill;
        configurationPanel.Padding = new Padding(14, 10, 14, 8);

        configurationLayout.ColumnCount = 1;
        configurationLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        configurationLayout.Controls.Add(primaryLayout, 0, 0);
        configurationLayout.Controls.Add(settingsPanel, 0, 1);
        configurationLayout.Dock = DockStyle.Fill;
        configurationLayout.RowCount = 2;
        configurationLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 50F));
        configurationLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));

        primaryLayout.ColumnCount = 10;
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 50F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 210F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 50F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 55F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 40F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 62F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 45F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 40F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 94F));
        primaryLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 78F));
        primaryLayout.Controls.Add(backendLabel, 0, 0);
        primaryLayout.Controls.Add(backendBox, 1, 0);
        primaryLayout.Controls.Add(manifestLabel, 2, 0);
        primaryLayout.Controls.Add(manifestText, 3, 0);
        primaryLayout.Controls.Add(manifestBrowseButton, 4, 0);
        primaryLayout.Controls.Add(runtimeLabel, 5, 0);
        primaryLayout.Controls.Add(runtimeRootText, 6, 0);
        primaryLayout.Controls.Add(runtimeBrowseButton, 7, 0);
        primaryLayout.Controls.Add(initializeButton, 8, 0);
        primaryLayout.Controls.Add(destroyButton, 9, 0);
        primaryLayout.Dock = DockStyle.Fill;

        ConfigureLabel(backendLabel, "后端");
        backendBox.Dock = DockStyle.Fill;
        backendBox.DropDownStyle = ComboBoxStyle.DropDownList;
        backendBox.Margin = new Padding(3, 5, 8, 7);
        ConfigureLabel(manifestLabel, "模型");
        manifestText.Dock = DockStyle.Fill;
        manifestText.Margin = new Padding(3, 6, 3, 7);
        ConfigureBrowseButton(manifestBrowseButton, "选择模型清单");
        ConfigureLabel(runtimeLabel, "运行库");
        runtimeRootText.Dock = DockStyle.Fill;
        runtimeRootText.Margin = new Padding(3, 6, 3, 7);
        ConfigureBrowseButton(runtimeBrowseButton, "选择 Runtime 根目录");
        ConfigureCommandButton(initializeButton, "初始化", true);
        ConfigureCommandButton(destroyButton, "销毁", false);

        settingsPanel.AutoScroll = true;
        settingsPanel.Controls.Add(limitSideLabel);
        settingsPanel.Controls.Add(limitSide);
        settingsPanel.Controls.Add(detThresholdLabel);
        settingsPanel.Controls.Add(detThreshold);
        settingsPanel.Controls.Add(boxThresholdLabel);
        settingsPanel.Controls.Add(boxThreshold);
        settingsPanel.Controls.Add(unclipRatioLabel);
        settingsPanel.Controls.Add(unclipRatio);
        settingsPanel.Controls.Add(recBatchLabel);
        settingsPanel.Controls.Add(recBatch);
        settingsPanel.Controls.Add(recConcurrencyLabel);
        settingsPanel.Controls.Add(recConcurrency);
        settingsPanel.Controls.Add(clsBatchLabel);
        settingsPanel.Controls.Add(clsBatch);
        settingsPanel.Controls.Add(useGpuCheck);
        settingsPanel.Controls.Add(deviceIdLabel);
        settingsPanel.Controls.Add(deviceId);
        settingsPanel.Controls.Add(classifierCheck);
        settingsPanel.Dock = DockStyle.Fill;
        settingsPanel.Padding = new Padding(0, 5, 0, 0);
        settingsPanel.WrapContents = false;

        ConfigureSettingLabel(limitSideLabel, "长边");
        ConfigureInteger(limitSide, 320, 4096, 960, 32);
        ConfigureSettingLabel(detThresholdLabel, "检测阈值");
        ConfigureDecimal(detThreshold, 0.01M, 0.99M, 0.30M);
        ConfigureSettingLabel(boxThresholdLabel, "框阈值");
        ConfigureDecimal(boxThreshold, 0.01M, 0.99M, 0.60M);
        ConfigureSettingLabel(unclipRatioLabel, "扩张比例");
        ConfigureDecimal(unclipRatio, 1.00M, 5.00M, 1.60M);
        ConfigureSettingLabel(recBatchLabel, "识别批次");
        ConfigureInteger(recBatch, 1, 64, 1, 1);
        ConfigureSettingLabel(recConcurrencyLabel, "识别并发");
        ConfigureInteger(recConcurrency, 1, 32, 2, 1);
        ConfigureSettingLabel(clsBatchLabel, "分类批次");
        ConfigureInteger(clsBatch, 1, 64, 8, 1);
        useGpuCheck.AutoSize = true;
        useGpuCheck.Margin = new Padding(16, 7, 4, 0);
        useGpuCheck.Text = "使用 GPU";
        ConfigureSettingLabel(deviceIdLabel, "GPU ID");
        deviceIdLabel.Margin = new Padding(8, 8, 3, 0);
        ConfigureInteger(deviceId, 0, 16, 0, 1);
        deviceId.Width = 54;
        classifierCheck.AutoSize = true;
        classifierCheck.Margin = new Padding(16, 7, 4, 0);
        classifierCheck.Text = "方向分类 CLS";

        workspacePanel.Controls.Add(workspaceSplit);
        workspacePanel.Dock = DockStyle.Fill;
        workspacePanel.Padding = new Padding(12, 10, 12, 10);

        workspaceSplit.BackColor = Color.FromArgb(216, 220, 223);
        workspaceSplit.Dock = DockStyle.Fill;
        workspaceSplit.Panel1.Controls.Add(imagePanel);
        workspaceSplit.Panel2.Controls.Add(resultPanel);
        workspaceSplit.SplitterWidth = 6;

        imagePanel.BackColor = Color.White;
        imagePanel.ColumnCount = 1;
        imagePanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        imagePanel.Controls.Add(imageToolbar, 0, 0);
        imagePanel.Controls.Add(imageView, 0, 1);
        imagePanel.Dock = DockStyle.Fill;
        imagePanel.RowCount = 2;
        imagePanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 52F));
        imagePanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));

        imageToolbar.Controls.Add(openImageButton);
        imageToolbar.Controls.Add(recognizeButton);
        imageToolbar.Controls.Add(imageHintLabel);
        imageToolbar.Dock = DockStyle.Fill;
        imageToolbar.Padding = new Padding(8);
        imageToolbar.WrapContents = false;
        ConfigureCommandButton(openImageButton, "选择图片", false);
        ConfigureCommandButton(recognizeButton, "开始识别", true);
        imageHintLabel.AutoSize = true;
        imageHintLabel.ForeColor = Color.FromArgb(100, 106, 110);
        imageHintLabel.Margin = new Padding(14, 8, 0, 0);
        imageHintLabel.Text = "支持 JPG / PNG / BMP；鼠标拖拽框选区域可直接识别";
        imageView.Dock = DockStyle.Fill;

        resultPanel.BackColor = Color.White;
        resultPanel.ColumnCount = 1;
        resultPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
        resultPanel.Controls.Add(resultToolbar, 0, 0);
        resultPanel.Controls.Add(resultTabs, 0, 1);
        resultPanel.Dock = DockStyle.Fill;
        resultPanel.RowCount = 2;
        resultPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 52F));
        resultPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));

        resultToolbar.Controls.Add(resultTitleLabel);
        resultToolbar.Controls.Add(copyButton);
        resultToolbar.Dock = DockStyle.Fill;
        resultToolbar.Padding = new Padding(10, 8, 8, 8);
        resultToolbar.WrapContents = false;
        resultTitleLabel.AutoSize = true;
        resultTitleLabel.Font = new Font("Microsoft YaHei UI", 9F, FontStyle.Bold);
        resultTitleLabel.Margin = new Padding(0, 8, 14, 0);
        resultTitleLabel.Text = "识别结果";
        ConfigureCommandButton(copyButton, "复制文本", false);

        resultTabs.Controls.Add(structuredTab);
        resultTabs.Controls.Add(textTab);
        resultTabs.Dock = DockStyle.Fill;
        structuredTab.Controls.Add(resultGrid);
        structuredTab.Text = "明细";
        resultGrid.AllowUserToAddRows = false;
        resultGrid.AllowUserToDeleteRows = false;
        resultGrid.AllowUserToResizeRows = false;
        resultGrid.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.AllCells;
        resultGrid.BackgroundColor = Color.White;
        resultGrid.BorderStyle = BorderStyle.None;
        resultGrid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize;
        resultGrid.Columns.AddRange(indexColumn, textColumn, scoreColumn);
        resultGrid.Dock = DockStyle.Fill;
        resultGrid.MultiSelect = true;
        resultGrid.ReadOnly = true;
        resultGrid.RowHeadersVisible = false;
        resultGrid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
        indexColumn.HeaderText = "#";
        indexColumn.ReadOnly = true;
        indexColumn.SortMode = DataGridViewColumnSortMode.NotSortable;
        indexColumn.Width = 42;
        textColumn.AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill;
        textColumn.DefaultCellStyle = new DataGridViewCellStyle { WrapMode = DataGridViewTriState.True };
        textColumn.HeaderText = "文字";
        textColumn.MinimumWidth = 140;
        textColumn.ReadOnly = true;
        textColumn.SortMode = DataGridViewColumnSortMode.NotSortable;
        scoreColumn.HeaderText = "置信度";
        scoreColumn.ReadOnly = true;
        scoreColumn.SortMode = DataGridViewColumnSortMode.NotSortable;
        scoreColumn.Width = 76;
        textTab.Controls.Add(plainText);
        textTab.Padding = new Padding(8);
        textTab.Text = "纯文本";
        plainText.BorderStyle = BorderStyle.None;
        plainText.Dock = DockStyle.Fill;
        plainText.Font = new Font("Microsoft YaHei UI", 10F);
        plainText.Multiline = true;
        plainText.ReadOnly = true;
        plainText.ScrollBars = ScrollBars.Both;

        statusBar.BackColor = Color.FromArgb(48, 54, 58);
        statusBar.ForeColor = Color.White;
        statusBar.Items.AddRange(new ToolStripItem[] { statusLabel, timingLabel });
        statusBar.SizingGrip = false;
        statusLabel.Spring = true;
        statusLabel.TextAlign = ContentAlignment.MiddleLeft;
        timingLabel.TextAlign = ContentAlignment.MiddleRight;

        AutoScaleDimensions = new SizeF(96F, 96F);
        AutoScaleMode = AutoScaleMode.Dpi;
        BackColor = Color.FromArgb(242, 244, 245);
        ClientSize = new Size(1420, 850);
        Controls.Add(rootLayout);
        MinimumSize = new Size(1100, 700);
        StartPosition = FormStartPosition.CenterScreen;
        Text = "lw.PPOCR.Inference";
        AllowDrop = true;

        rootLayout.ResumeLayout(false);
        rootLayout.PerformLayout();
        configurationPanel.ResumeLayout(false);
        configurationLayout.ResumeLayout(false);
        primaryLayout.ResumeLayout(false);
        primaryLayout.PerformLayout();
        settingsPanel.ResumeLayout(false);
        settingsPanel.PerformLayout();
        ((System.ComponentModel.ISupportInitialize)limitSide).EndInit();
        ((System.ComponentModel.ISupportInitialize)detThreshold).EndInit();
        ((System.ComponentModel.ISupportInitialize)boxThreshold).EndInit();
        ((System.ComponentModel.ISupportInitialize)unclipRatio).EndInit();
        ((System.ComponentModel.ISupportInitialize)recBatch).EndInit();
        ((System.ComponentModel.ISupportInitialize)recConcurrency).EndInit();
        ((System.ComponentModel.ISupportInitialize)clsBatch).EndInit();
        ((System.ComponentModel.ISupportInitialize)deviceId).EndInit();
        workspacePanel.ResumeLayout(false);
        workspaceSplit.Panel1.ResumeLayout(false);
        workspaceSplit.Panel2.ResumeLayout(false);
        ((System.ComponentModel.ISupportInitialize)workspaceSplit).EndInit();
        workspaceSplit.ResumeLayout(false);
        imagePanel.ResumeLayout(false);
        imageToolbar.ResumeLayout(false);
        imageToolbar.PerformLayout();
        resultPanel.ResumeLayout(false);
        resultToolbar.ResumeLayout(false);
        resultToolbar.PerformLayout();
        resultTabs.ResumeLayout(false);
        structuredTab.ResumeLayout(false);
        ((System.ComponentModel.ISupportInitialize)resultGrid).EndInit();
        textTab.ResumeLayout(false);
        textTab.PerformLayout();
        statusBar.ResumeLayout(false);
        statusBar.PerformLayout();
        ResumeLayout(false);
    }

    private void ConfigureBrowseButton(Button button, string toolTip)
    {
        button.Dock = DockStyle.Fill;
        button.FlatStyle = FlatStyle.Flat;
        button.Margin = new Padding(4, 2, 4, 2);
        button.TabStop = false;
        button.Text = "...";
        button.FlatAppearance.BorderColor = Color.FromArgb(190, 196, 200);
        browseToolTip.SetToolTip(button, toolTip);
    }

    private static void ConfigureCommandButton(Button button, string text, bool primary)
    {
        button.BackColor = primary ? Color.FromArgb(0, 122, 92) : Color.White;
        button.FlatStyle = FlatStyle.Flat;
        button.ForeColor = primary ? Color.White : Color.FromArgb(46, 52, 56);
        button.Margin = new Padding(4, 4, 4, 7);
        button.Size = new Size(primary ? 90 : 74, 32);
        button.Text = text;
        button.FlatAppearance.BorderColor = primary
            ? Color.FromArgb(0, 122, 92)
            : Color.FromArgb(184, 190, 194);
    }

    private static void ConfigureLabel(Label label, string text)
    {
        label.Dock = DockStyle.Fill;
        label.ForeColor = Color.FromArgb(61, 67, 71);
        label.Text = text;
        label.TextAlign = ContentAlignment.MiddleLeft;
    }

    private static void ConfigureSettingLabel(Label label, string text)
    {
        label.AutoSize = true;
        label.ForeColor = Color.FromArgb(75, 81, 85);
        label.Margin = new Padding(8, 8, 3, 0);
        label.Text = text;
    }

    private static void ConfigureInteger(
        NumericUpDown input, decimal min, decimal max, decimal value, decimal step)
    {
        input.DecimalPlaces = 0;
        input.Increment = step;
        input.Margin = new Padding(0, 3, 5, 0);
        input.Maximum = max;
        input.Minimum = min;
        input.TextAlign = HorizontalAlignment.Right;
        input.Value = value;
        input.Width = 66;
    }

    private static void ConfigureDecimal(
        NumericUpDown input, decimal min, decimal max, decimal value)
    {
        input.DecimalPlaces = 2;
        input.Increment = 0.05M;
        input.Margin = new Padding(0, 3, 5, 0);
        input.Maximum = max;
        input.Minimum = min;
        input.TextAlign = HorizontalAlignment.Right;
        input.Value = value;
        input.Width = 66;
    }

    private TableLayoutPanel rootLayout = null!;
    private Panel configurationPanel = null!;
    private TableLayoutPanel configurationLayout = null!;
    private TableLayoutPanel primaryLayout = null!;
    private Label backendLabel = null!;
    private ComboBox backendBox = null!;
    private Label manifestLabel = null!;
    private TextBox manifestText = null!;
    private Button manifestBrowseButton = null!;
    private Label runtimeLabel = null!;
    private TextBox runtimeRootText = null!;
    private Button runtimeBrowseButton = null!;
    private Button initializeButton = null!;
    private Button destroyButton = null!;
    private FlowLayoutPanel settingsPanel = null!;
    private Label limitSideLabel = null!;
    private NumericUpDown limitSide = null!;
    private Label detThresholdLabel = null!;
    private NumericUpDown detThreshold = null!;
    private Label boxThresholdLabel = null!;
    private NumericUpDown boxThreshold = null!;
    private Label unclipRatioLabel = null!;
    private NumericUpDown unclipRatio = null!;
    private Label recBatchLabel = null!;
    private NumericUpDown recBatch = null!;
    private Label recConcurrencyLabel = null!;
    private NumericUpDown recConcurrency = null!;
    private Label clsBatchLabel = null!;
    private NumericUpDown clsBatch = null!;
    private CheckBox useGpuCheck = null!;
    private Label deviceIdLabel = null!;
    private NumericUpDown deviceId = null!;
    private CheckBox classifierCheck = null!;
    private Panel workspacePanel = null!;
    private SplitContainer workspaceSplit = null!;
    private TableLayoutPanel imagePanel = null!;
    private FlowLayoutPanel imageToolbar = null!;
    private Button openImageButton = null!;
    private Button recognizeButton = null!;
    private Label imageHintLabel = null!;
    private OcrImageView imageView = null!;
    private TableLayoutPanel resultPanel = null!;
    private FlowLayoutPanel resultToolbar = null!;
    private Label resultTitleLabel = null!;
    private Button copyButton = null!;
    private TabControl resultTabs = null!;
    private TabPage structuredTab = null!;
    private DataGridView resultGrid = null!;
    private DataGridViewTextBoxColumn indexColumn = null!;
    private DataGridViewTextBoxColumn textColumn = null!;
    private DataGridViewTextBoxColumn scoreColumn = null!;
    private TabPage textTab = null!;
    private TextBox plainText = null!;
    private StatusStrip statusBar = null!;
    private ToolStripStatusLabel statusLabel = null!;
    private ToolStripStatusLabel timingLabel = null!;
    private ToolTip browseToolTip = null!;
}
