# Frame grabber for the showcase videos (dot-sourced by showcase.ps1). A background thread
# PrintWindow-captures the game's client area as fast as it can, downscales each frame to
# 1280x720 and writes it as a JPEG with its timestamp; Stop-FrameCapture writes an ffmpeg concat
# list with each frame's real duration, so the clip can be encoded at true speed:
#   ffmpeg -f concat -safe 0 -i frames.txt -vf fps=30 ...
if (-not ('BsShowcase.Capture' -as [type])) {
    Add-Type -AssemblyName System.Drawing
    $refs = @('System.Drawing.Common', 'System.Drawing.Primitives', 'System.Collections', 'System.Threading', 'System.Threading.Thread', 'System.Runtime.InteropServices', 'System.Linq', 'System.Private.Windows.Core', 'System.Private.Windows.GdiPlus', 'System.ComponentModel.Primitives') |
        Where-Object { try { [void][Reflection.Assembly]::Load($_); $true } catch { $false } }
    Add-Type -ReferencedAssemblies $refs -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

namespace BsShowcase {
public static class Capture {
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] struct POINT { public int X, Y; }
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref POINT p);

    static Thread thread;
    static volatile bool stop;
    static List<KeyValuePair<string, double>> frames = new List<KeyValuePair<string, double>>();
    public static string LastError;

    public static void Start(IntPtr hwnd, string dir, int outW, int outH, int maxFrames) {
        Directory.CreateDirectory(dir);
        frames = new List<KeyValuePair<string, double>>();
        stop = false; LastError = null;
        thread = new Thread(() => Run(hwnd, dir, outW, outH, maxFrames)) { IsBackground = true, Priority = ThreadPriority.AboveNormal };
        thread.Start();
    }

    static void Run(IntPtr hwnd, string dir, int outW, int outH, int maxFrames) {
        try {
            var codec = ImageCodecInfo.GetImageEncoders().First(c => c.MimeType == "image/jpeg");
            var ep = new EncoderParameters(1);
            ep.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, 94L);
            var sw = Stopwatch.StartNew();
            using (var outBmp = new Bitmap(outW, outH, PixelFormat.Format24bppRgb))
            using (var og = Graphics.FromImage(outBmp)) {
                og.InterpolationMode = InterpolationMode.HighQualityBilinear;
                og.PixelOffsetMode = PixelOffsetMode.HighQuality;
                while (!stop && frames.Count < maxFrames) {
                    RECT wr; GetWindowRect(hwnd, out wr);
                    RECT cr; GetClientRect(hwnd, out cr);
                    var pt = new POINT(); ClientToScreen(hwnd, ref pt);
                    int ww = wr.Right - wr.Left, wh = wr.Bottom - wr.Top;
                    double t = sw.Elapsed.TotalMilliseconds;
                    using (var full = new Bitmap(ww, wh, PixelFormat.Format32bppArgb)) {
                        using (var g = Graphics.FromImage(full)) {
                            IntPtr hdc = g.GetHdc();
                            PrintWindow(hwnd, hdc, 2);
                            g.ReleaseHdc(hdc);
                        }
                        og.DrawImage(full, new Rectangle(0, 0, outW, outH),
                            new Rectangle(pt.X - wr.Left, pt.Y - wr.Top, cr.Right - cr.Left, cr.Bottom - cr.Top), GraphicsUnit.Pixel);
                    }
                    string name = Path.Combine(dir, string.Format("f{0:D5}.jpg", frames.Count));
                    outBmp.Save(name, codec, ep);
                    frames.Add(new KeyValuePair<string, double>(name, t));
                }
            }
        } catch (Exception e) { LastError = e.ToString(); }
    }

    // Stops the thread and writes the concat list; returns the number of frames.
    public static int Stop(string listPath, out double seconds) {
        stop = true;
        if (thread != null) thread.Join();
        var lines = new List<string>();
        seconds = 0;
        for (int i = 0; i < frames.Count; i++) {
            double d = i + 1 < frames.Count ? (frames[i + 1].Value - frames[i].Value) / 1000.0 : 0.05;
            lines.Add("file '" + frames[i].Key.Replace("\\", "/") + "'");
            lines.Add("duration " + d.ToString("0.0000", System.Globalization.CultureInfo.InvariantCulture));
            seconds += d;
        }
        if (frames.Count > 0) lines.Add("file '" + frames[frames.Count - 1].Key.Replace("\\", "/") + "'");
        File.WriteAllLines(listPath, lines);
        return frames.Count;
    }
}
}
'@
}

# Start-FrameCapture 'teleport' -> frames in out/<scenario>/frames-teleport/
function Start-FrameCapture([string]$name, [int]$MaxFrames = 900) {
    $dir = Join-Path (Join-Path $script:OutRoot $script:Unit) "frames-$name"
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    [BsShowcase.Capture]::Start((Get-GameWindow), $dir, 1280, 720, $MaxFrames)
    $script:CaptureDir = $dir
    Write-Step "capture '$name' started"
}

function Stop-FrameCapture {
    $sec = 0.0
    $n = [BsShowcase.Capture]::Stop((Join-Path $script:CaptureDir 'frames.txt'), [ref]$sec)
    if ([BsShowcase.Capture]::LastError) { Write-Step "capture error: $([BsShowcase.Capture]::LastError)" }
    Write-Step ("capture stopped: {0} frames in {1:0.0} s ({2:0.0} fps) -> {3}" -f $n, $sec, ($n / [Math]::Max($sec, 0.001)), $script:CaptureDir)
}
