using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

namespace HotD2VRSetup {
public static class Program {
    public static byte[] Payload() {using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("HotD2VR.Payload"))using(var m=new MemoryStream()){if(s==null)throw new IOException("Mod package missing.");s.CopyTo(m);return m.ToArray();}}
    public static void Open(string path) {Process.Start(new ProcessStartInfo(path){UseShellExecute=true});}
    public static string DefaultRoot() {
        using(var k=Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\HotD2VR")) {
            string saved=k==null?null:k.GetValue("InstallLocation") as string;
            if(!String.IsNullOrEmpty(saved)&&Directory.Exists(saved))return saved;
        }
        return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"HotD2VR");
    }
    [STAThread] public static int Main(string[] args) {
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        try {
            string exe=Assembly.GetExecutingAssembly().Location;
            if(args.Length>0&&args[0]=="/play") {Play(Path.GetDirectoryName(exe));return 0;}
            if(args.Length==2&&args[0]=="/render-preview") {
                using(var f=new SetupForm()) {f.CreateControl();f.Show();Application.DoEvents();using(var b=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(b,new Rectangle(0,0,b.Width,b.Height));b.Save(args[1]);}f.Hide();}return 0;
            }
            if(args.Length==2&&args[0]=="/uninstall") {
                InstallerCore.Owned(args[1]);InstallerCore.Idle();
                if(MessageBox.Show("Remove the HotD2VR mod? Your original game, copied game/saves, settings and backups will be kept.","Remove HotD2VR",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return 0;
                string temp=Path.Combine(Path.GetTempPath(),"HotD2VR-remove-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temp);string copy=Path.Combine(temp,"HotD2VR.exe");File.Copy(exe,copy);
                Process.Start(new ProcessStartInfo(copy,"/remove-worker "+InstallerCore.Quote(InstallerCore.Full(args[1]))){UseShellExecute=false,WindowStyle=ProcessWindowStyle.Hidden});return 0;
            }
            if(args.Length==2&&args[0]=="/remove-worker") {
                // The launching copy exits first, releasing the installed executable.
                string root=InstallerCore.Full(args[1]);string target=InstallerCore.Under(root,"HotD2VR.exe");
                for(int n=0;n<40&&File.Exists(target);++n){try{using(var f=new FileStream(target,FileMode.Open,FileAccess.ReadWrite,FileShare.None)){}break;}catch(IOException){Thread.Sleep(100);}}
                MessageBox.Show(InstallerCore.Uninstall(root,true),"HotD2VR removed",MessageBoxButtons.OK,MessageBoxIcon.Information);return 0;
            }
            if(args.Length!=0)throw new IOException("Unsupported setup argument.");
            Application.Run(new SetupForm());return 0;
        } catch(Exception e){MessageBox.Show(e.Message,"HotD2VR",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
    }
    public static void Play(string root) {
        using(var mutex=InstallerCore.OperationMutex(root)) {
            bool acquired=false;
            try {try{acquired=mutex.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
                if(!acquired)throw new IOException("Setup or another launch is already using this installation. Wait for it to finish, then play again.");
                PlayLocked(root);
            }finally{if(acquired)mutex.ReleaseMutex();}
        }
    }
    static void PlayLocked(string root) {
        var record=InstallerCore.Owned(root);InstallerCore.Idle();
        if(InstallerCore.RuntimeStatus().StartsWith("No active",StringComparison.Ordinal))throw new IOException(InstallerCore.RuntimeStatus()+"\n\nThen launch Play HotD2VR again. Setup help: "+InstallerCore.HelpUrl);
        string script=InstallerCore.Under(root,"pcvr/run-probe.ps1");
        string disc=record.ContainsKey("disc")?record["disc"] as string:"";InstallerCore.ValidateDisc(disc);
        string arguments="-NoProfile -ExecutionPolicy Bypass -File "+InstallerCore.Quote(script)+" -VR -TexturePack";
        if(!String.IsNullOrEmpty(disc))arguments+=" -DiscImage "+InstallerCore.Quote(disc);
        string shell=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),@"WindowsPowerShell\v1.0\powershell.exe");
        string log=InstallerCore.Under(root,"working/pcvr/launcher.log");Directory.CreateDirectory(Path.GetDirectoryName(log));
        using(var writer=new StreamWriter(log,false)) using(var p=new Process()) {
            p.StartInfo=new ProcessStartInfo(shell,arguments){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=root,RedirectStandardOutput=true,RedirectStandardError=true};
            object gate=new object();DataReceivedEventHandler output=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null)lock(gate){writer.WriteLine(e.Data);writer.Flush();}};
            p.OutputDataReceived+=output;p.ErrorDataReceived+=output;p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();
            if(p.ExitCode!=0)throw new IOException("The game could not start or exited with an error.\n\n"+File.ReadAllText(log)+"\n\nLog: "+log);
        }
    }
}
public sealed class SetupForm:Form {
    readonly TextBox source=new TextBox(),destination=new TextBox(),disc=new TextBox();
    readonly CheckBox shortcut=new CheckBox();readonly Label status=new Label();readonly ProgressBar progress=new ProgressBar();
    readonly Button install=new Button(),cancel=new Button(),play=new Button();
    readonly BackgroundWorker worker=new BackgroundWorker();CancellationTokenSource cancellation;bool busy=false;string installed;
    public SetupForm() {
        Text="HotD2VR Setup — Alpha 23";Font=new Font("Segoe UI",10);ClientSize=new Size(760,605);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;AutoScaleMode=AutoScaleMode.Dpi;BackColor=Color.FromArgb(246,248,251);
        Label title=LabelAt("Bring the arcade into VR",24,22,710,38);title.Font=new Font("Segoe UI",22,FontStyle.Bold);title.ForeColor=Color.FromArgb(30,48,69);
        LabelAt("HotD2VR "+InstallerCore.Release+"  ·  Original Windows PC game required",26,66,710,26);
        LabelAt("Select your own game. Setup creates a separate playable copy and downloads\nverified graphics/OpenXR dependencies. No game files are included.",26,104,710,47);
        Field("Original game folder (contains Hod2.exe)",source,169,delegate {using(var d=new FolderBrowserDialog()){d.Description="Choose your installed original PC game folder containing Hod2.exe";d.ShowNewFolderButton=false;if(d.ShowDialog()==DialogResult.OK)source.Text=d.SelectedPath;}});
        Field("Install HotD2VR to",destination,249,delegate {using(var d=new FolderBrowserDialog()){d.Description="Choose a dedicated empty HotD2VR folder, or an existing HotD2VR installation";if(d.ShowDialog()==DialogResult.OK)destination.Text=d.SelectedPath;}});
        Field("Original disc ISO (optional — leave blank for physical / mounted media)",disc,329,delegate {using(var d=new OpenFileDialog()){d.Filter="Original game ISO (*.iso)|*.iso";if(d.ShowDialog()==DialogResult.OK)disc.Text=d.FileName;}});
        destination.Text=Program.DefaultRoot();
        try {var m=InstallerCore.Owned(destination.Text);source.Text=Path.Combine(destination.Text,@"working\pcvr\game");disc.Text=m["disc"] as string;}catch(Exception){}
        shortcut.SetBounds(26,410,320,26);shortcut.Text="Create a Play HotD2VR desktop shortcut";shortcut.Checked=true;Controls.Add(shortcut);
        var help=ButtonAt("Setup help",380,406,125,32);help.Click+=delegate {Program.Open(InstallerCore.HelpUrl);};
        var find=ButtonAt("Find game files",517,406,216,32);find.Click+=delegate {Program.Open(InstallerCore.GameUrl);};
        status.SetBounds(26,453,708,56);status.Text=InstallerCore.RuntimeStatus();status.ForeColor=Color.FromArgb(66,83,105);Controls.Add(status);
        progress.SetBounds(26,518,708,14);Controls.Add(progress);
        install.SetBounds(26,549,180,36);install.Text="Install / update";install.Click+=BeginInstall;Controls.Add(install);
        play.SetBounds(220,549,165,36);play.Text="Play HotD2VR";play.Enabled=false;play.Click+=delegate {Process.Start(new ProcessStartInfo(Path.Combine(installed,"HotD2VR.exe"),"/play"){UseShellExecute=false});Close();};Controls.Add(play);
        cancel.SetBounds(609,549,125,36);cancel.Text="Close";cancel.Click+=delegate {if(busy){cancellation.Cancel();status.Text="Cancelling setup...";}else Close();};Controls.Add(cancel);
        AcceptButton=install;CancelButton=cancel;
        worker.WorkerReportsProgress=true;
        worker.DoWork+=delegate(object sender,DoWorkEventArgs e){e.Result=InstallerCore.Install((InstallOptions)e.Argument,Program.Payload(),Assembly.GetExecutingAssembly().Location,delegate(int value,string message){worker.ReportProgress(value,message);},cancellation.Token);};
        worker.ProgressChanged+=delegate(object sender,ProgressChangedEventArgs e){progress.Value=e.ProgressPercentage;status.Text=(string)e.UserState;};
        worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e){
            busy=false;cancel.Text="Close";source.Enabled=destination.Enabled=disc.Enabled=install.Enabled=shortcut.Enabled=true;
            if(e.Error!=null){status.Text=cancellation.IsCancellationRequested?"Setup cancelled. Your original game was not changed.":"Setup could not finish. See the message for details.";if(!cancellation.IsCancellationRequested)MessageBox.Show(this,e.Error.Message,"Setup could not finish",MessageBoxButtons.OK,MessageBoxIcon.Error);}
            else {installed=InstallerCore.Full(destination.Text);play.Enabled=true;status.Text="Installed. Connect your headset through Virtual Desktop, then choose Play HotD2VR.";MessageBox.Show(this,"HotD2VR is ready.\n\n"+e.Result+"\n\n"+InstallerCore.RuntimeStatus()+"\n\nLeft Y toggles aiming markers; B or lowering the gun reloads.","Setup complete",MessageBoxButtons.OK,MessageBoxIcon.Information);}
            cancellation.Dispose();
        };
        FormClosing+=delegate(object sender,FormClosingEventArgs e){if(busy){e.Cancel=true;cancellation.Cancel();status.Text="Cancelling setup...";}};
    }
    Label LabelAt(string text,int x,int y,int w,int h){var l=new Label(){Text=text};l.SetBounds(x,y,w,h);Controls.Add(l);return l;}
    Button ButtonAt(string text,int x,int y,int w,int h){var b=new Button(){Text=text};b.SetBounds(x,y,w,h);Controls.Add(b);return b;}
    void Field(string label,TextBox field,int y,Action browse){LabelAt(label,26,y,710,23);field.SetBounds(26,y+28,578,29);Controls.Add(field);var b=ButtonAt("Browse…",617,y+26,117,32);b.Click+=delegate {if(!busy)browse();};}
    void BeginInstall(object sender,EventArgs e) {
        try {
            InstallerCore.Idle();if(String.IsNullOrWhiteSpace(source.Text)||String.IsNullOrWhiteSpace(destination.Text))throw new IOException("Choose your original game folder and installation destination first.");
            cancellation=new CancellationTokenSource();busy=true;install.Enabled=play.Enabled=source.Enabled=destination.Enabled=disc.Enabled=shortcut.Enabled=false;cancel.Text="Cancel setup";
            worker.RunWorkerAsync(new InstallOptions(){Source=source.Text,Destination=destination.Text,Disc=disc.Text,Shortcut=shortcut.Checked,Register=true,Cache=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"HotD2VR-downloads")});
        } catch(Exception error){MessageBox.Show(this,error.Message,"HotD2VR Setup",MessageBoxButtons.OK,MessageBoxIcon.Error);}
    }
}
}
