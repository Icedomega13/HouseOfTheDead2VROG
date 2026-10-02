using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using Microsoft.Win32;
using HotD2VRSetup;

public static class InstallerTests {
    static int checks;
    static void Check(bool value,string name){++checks;if(!value)throw new Exception("FAIL: "+name);Console.WriteLine("PASS "+name);}
    static void Reject(Action action,string name){bool rejected=false;try{action();}catch(IOException){rejected=true;}Check(rejected,name);}
    static Dictionary<string,string> Inventory(string folder){return Directory.GetFiles(folder,"*",SearchOption.AllDirectories).ToDictionary(p=>p.Substring(folder.Length+1),p=>InstallerCore.Hash(p));}
    static byte[] Package(string version) {
        using(var stream=new MemoryStream()) {
            var hashes=new Dictionary<string,string>();
            using(var zip=new ZipArchive(stream,ZipArchiveMode.Create,true)) {
                foreach(string name in InstallerCore.PayloadFiles) {
                    var entry=zip.CreateEntry(name);string text=name=="pcvr/vr-settings.json"?"{\"EyeSize\":1200,\"AimingCursor\":true}":version+" "+name;
                    byte[] bytes=Encoding.UTF8.GetBytes(text);using(var output=entry.Open())output.Write(bytes,0,bytes.Length);
                    using(var hash=System.Security.Cryptography.SHA256.Create())hashes[name]=BitConverter.ToString(hash.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();
                }
                using(var output=new StreamWriter(zip.CreateEntry("package.json").Open()))output.Write(InstallerCore.Json.Serialize(hashes));
            }
            return stream.ToArray();
        }
    }
    static InstallOptions Options(string source,string target,string cache){return new InstallOptions{Source=source,Destination=target,Cache=cache,Disc="",Shortcut=false,Register=false};}
    static void Progress(int value,string text){}
    public static int Main(string[] args) {
        try {
            if(args.Length==5&&args[0]=="/production-install") {
                string productionSetup=InstallerCore.Full(args[4]);byte[] payload;
                var assembly=System.Reflection.Assembly.LoadFile(productionSetup);
                using(var stream=assembly.GetManifestResourceStream("HotD2VR.Payload"))using(var memory=new MemoryStream()){stream.CopyTo(memory);payload=memory.ToArray();}
                InstallerCore.Install(Options(args[1],args[2],args[3]),payload,productionSetup,Progress,CancellationToken.None);
                Console.WriteLine("PASS actual production payload installed from user-supplied game, with registry/desktop writes disabled.");return 0;
            }
            if(args.Length!=3)throw new Exception("Usage: InstallerTests <fresh-workspace-test-folder> <verified-dependency-cache> <setup-exe>");
            string home=InstallerCore.Full(args[0]),cache=InstallerCore.Full(args[1]),setup=args[2];InstallerCore.NoLinks(home);
            if(Directory.Exists(home))throw new Exception("Tests refuse to overwrite an existing folder.");Directory.CreateDirectory(home);
            string original=Path.Combine(home,"Original game — test"),target=Path.Combine(home,"Installed VR — test");Directory.CreateDirectory(original);
            File.WriteAllText(Path.Combine(original,"Hod2.exe"),"Nonexecutable fixture for installer tests only");
            File.WriteAllText(Path.Combine(original,"Hod2.ini"),"player configuration");
            foreach(string name in InstallerCore.DataFolders){Directory.CreateDirectory(Path.Combine(original,name));File.WriteAllText(Path.Combine(original,name,"data.bin"),name+" fixture");}
            string expected=InstallerCore.Hash(Path.Combine(original,"Hod2.exe"));var before=Inventory(original);
            Check(InstallerCore.GameFiles(original,expected).Count==9,"complete fixture game recognized");
            Reject(delegate {InstallerCore.GameFiles(original,InstallerCore.GameHash);},"unsupported executable rejected");
            File.Move(Path.Combine(original,"tex/data.bin"),Path.Combine(home,"texture.tmp"));
            Reject(delegate {InstallerCore.GameFiles(original,expected);},"empty data folder rejected");File.Move(Path.Combine(home,"texture.tmp"),Path.Combine(original,"tex/data.bin"));
            Reject(delegate {InstallerCore.Under(home,"../escape.txt");},"path traversal rejected");
            Reject(delegate {InstallerCore.Under(home,Path.Combine(Path.GetPathRoot(home),"escaped.txt"));},"absolute payload path rejected");
            Reject(delegate {InstallerCore.RemoveStage(original,home);},"cleanup refuses a non-staging folder");
            Reject(delegate {InstallerCore.ValidateDisc(Path.Combine(home,"missing.iso"));},"missing ISO rejected");
            File.WriteAllText(Path.Combine(home,"disc.zip"),"not ISO");Reject(delegate {InstallerCore.ValidateDisc(Path.Combine(home,"disc.zip"));},"non-ISO media rejected");
            Reject(delegate {InstallerCore.Install(Options(original,original,cache),Package("v1"),setup,Progress,CancellationToken.None,expected);},"source is never overwritten");
            string unrelated=Path.Combine(home,"Unrelated");Directory.CreateDirectory(unrelated);File.WriteAllText(Path.Combine(unrelated,"keep.txt"),"keep");
            Reject(delegate {InstallerCore.Install(Options(original,unrelated,cache),Package("v1"),setup,Progress,CancellationToken.None,expected);},"nonempty unrelated destination rejected");
            var cancelled=new CancellationTokenSource();cancelled.Cancel();bool cancellation=false;
            try{InstallerCore.Install(Options(original,target,cache),Package("v1"),setup,Progress,cancelled.Token,expected);}catch(OperationCanceledException){cancellation=true;}
            Check(cancellation&&!Directory.Exists(target),"cancelled validation commits nothing");
            string corrupt=Path.Combine(home,"Corrupt cache");Directory.CreateDirectory(corrupt);File.WriteAllText(Path.Combine(corrupt,InstallerCore.DgArchive),"bad");
            Reject(delegate {InstallerCore.Install(Options(original,target,corrupt),Package("v1"),setup,Progress,CancellationToken.None,expected);},"corrupt dependency blocks installation");
            Check(!Directory.Exists(target),"download failure commits no destination");
            Check(Directory.GetDirectories(home,".HotD2VR-stage-*").Length==0,"failed staging cleaned");
            var midCancel=new CancellationTokenSource();bool midCancelled=false;
            try {InstallerCore.Install(Options(original,target,cache),Package("v1"),setup,delegate(int value,string message){if(value>=35)midCancel.Cancel();},midCancel.Token,expected);}catch(OperationCanceledException){midCancelled=true;}
            Check(midCancelled&&!Directory.Exists(target)&&Directory.GetDirectories(home,".HotD2VR-stage-*").Length==0,"cancel during game copy leaves no partial install");
            using(var ready=new ManualResetEvent(false))using(var done=new ManualResetEvent(false)) {
                var owner=new Thread(delegate(){using(var mutex=InstallerCore.OperationMutex(target)){mutex.WaitOne();ready.Set();done.WaitOne();mutex.ReleaseMutex();}});owner.Start();ready.WaitOne();
                try {Reject(delegate{InstallerCore.Install(Options(original,target,cache),Package("v1"),setup,Progress,CancellationToken.None,expected);},"active setup/play mutex blocks a concurrent install");}
                finally{done.Set();owner.Join();}
            }
            byte[] damaged=Package("v1");
            using(var bytes=new MemoryStream()) {
                bytes.Write(damaged,0,damaged.Length);bytes.Position=0;
                using(var zip=new ZipArchive(bytes,ZipArchiveMode.Update,true)){zip.GetEntry("LICENSE").Delete();using(var output=new StreamWriter(zip.CreateEntry("LICENSE").Open()))output.Write("changed license payload");}
                string unpack=Path.Combine(home,"Corrupt payload");Directory.CreateDirectory(unpack);
                Reject(delegate{InstallerCore.ExtractPayload(bytes.ToArray(),unpack);},"tampered embedded payload rejected");
            }
            InstallerCore.Install(Options(original,target,cache),Package("v1"),setup,Progress,CancellationToken.None,expected);
            Check(File.Exists(Path.Combine(target,"install.json")),"first install has ownership record");
            Check(InstallerCore.Hash(Path.Combine(target,"working/pcvr/game/Hod2.exe"))==expected,"copied executable unchanged");
            Check(Inventory(original).All(p=>before[p.Key]==p.Value)&&Inventory(original).Count==before.Count,"original game untouched");
            Check(File.Exists(Path.Combine(target,"build/pcvr/openxr_loader.dll"))&&File.Exists(Path.Combine(target,"working/pcvr/game/ddraw_backend.dll")),"verified x86 runtime dependencies staged");
            string backend=File.ReadAllText(Path.Combine(target,"working/pcvr/game/dgVoodoo.conf"));Check(backend.Contains("OutputAPI = d3d11_fl11_0")&&backend.Contains("FullScreenMode = false")&&backend.Contains("CaptureMouse = false"),"backend uses production windowed settings");
            string settings=Path.Combine(target,"pcvr/vr-settings.json"),save=Path.Combine(target,"working/pcvr/game/player-save.dat");File.WriteAllText(settings,"{\"EyeSize\":800,\"AimingCursor\":false}");File.WriteAllText(save,"saved progress");
            string old=InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"));
            var options=Options(Path.Combine(target,"working/pcvr/game"),target,cache);
            InstallerCore.Install(options,Package("v2"),setup,Progress,CancellationToken.None,expected);
            Check(InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))!=old,"update replaces mod DLL");
            Check(File.ReadAllText(settings).Contains("800")&&File.ReadAllText(save)=="saved progress","update preserves preferences and saves");
            string[] backups=Directory.GetDirectories(Path.Combine(target,"backups"));Check(backups.Length==1&&InstallerCore.Hash(Path.Combine(backups[0],"build/pcvr/ddraw.dll"))==old,"previous mod preserved before update");
            Check(Directory.GetDirectories(home,".HotD2VR-stage-*").Length==0,"successful update staging cleaned");
            string current=InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"));
            using(var locked=new FileStream(Path.Combine(target,"build/pcvr/ddraw.dll"),FileMode.Open,FileAccess.ReadWrite,FileShare.None))Reject(delegate{InstallerCore.Install(options,Package("v3"),setup,Progress,CancellationToken.None,expected);},"locked mod aborts update before replacements");
            Check(InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))==current,"failed preflight preserves current build");
            // Force a replacement failure after earlier writes. The real engine must
            // restore them from its preflight backup, not merely reject validation.
            File.Delete(Path.Combine(target,"LICENSE"));Directory.CreateDirectory(Path.Combine(target,"LICENSE"));
            Reject(delegate{InstallerCore.Install(options,Package("v3"),setup,Progress,CancellationToken.None,expected);},"mid-update write failure rejected");
            Check(InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))==current,"mid-update rollback restores previous DLL");
            Directory.Delete(Path.Combine(target,"LICENSE"));File.WriteAllText(Path.Combine(target,"LICENSE"),"v2 LICENSE");
            InstallerCore.Install(options,Package("v1"),setup,Progress,CancellationToken.None,expected);
            Check(InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))==old,"earlier installer rolls mod back");
            string keyPath=@"Software\HotD2VR-InstallerTests\"+Guid.NewGuid().ToString("N"),shortcut=Path.Combine(home,"Play HotD2VR.lnk");
            try {
                InstallerCore.CreateShortcut(target,shortcut);Check(File.Exists(shortcut),"actual Windows shortcut created inside test workspace");
                Type shellType=Type.GetTypeFromProgID("WScript.Shell");object shell=Activator.CreateInstance(shellType);object link=shellType.InvokeMember("CreateShortcut",System.Reflection.BindingFlags.InvokeMethod,null,shell,new object[]{shortcut});
                string linkTarget=(string)link.GetType().InvokeMember("TargetPath",System.Reflection.BindingFlags.GetProperty,null,link,null);
                string linkArguments=(string)link.GetType().InvokeMember("Arguments",System.Reflection.BindingFlags.GetProperty,null,link,null);
                Check(InstallerCore.Same(linkTarget,Path.Combine(target,"HotD2VR.exe"))&&linkArguments=="/play","shortcut targets packaged play entry");
                System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
                InstallerCore.Register(target,keyPath);
                using(var key=Registry.CurrentUser.OpenSubKey(keyPath)){Check((string)key.GetValue("DisplayVersion")==InstallerCore.Release&&((string)key.GetValue("UninstallString")).Contains("/uninstall"),"actual isolated registry removal entry configured");}
                string linkHash=InstallerCore.Hash(shortcut);
                Reject(delegate{InstallerCore.CreateShortcut(unrelated,shortcut);},"another installation's shortcut cannot be overwritten");
                Check(InstallerCore.Hash(shortcut)==linkHash,"existing shortcut preserved after rejected replacement");
                Reject(delegate{InstallerCore.Register(unrelated,keyPath);},"another installation's removal entry cannot be overwritten");
                InstallerCore.RemoveIntegration(unrelated,keyPath,shortcut);
                using(var key=Registry.CurrentUser.OpenSubKey(keyPath)){Check(key!=null&&File.Exists(shortcut),"removal respects another installation's integration ownership");}
                InstallerCore.RemoveIntegration(target,keyPath,shortcut);
                using(var key=Registry.CurrentUser.OpenSubKey(keyPath)){Check(key==null&&!File.Exists(shortcut),"owned shortcut and isolated removal entry cleaned");}
            } finally {InstallerCore.RemoveIntegration(target,keyPath,shortcut);}
            var record=InstallerCore.Owned(target);var owned=InstallerCore.Json.ConvertToType<Dictionary<string,string>>(record["files"]);owned["working/pcvr/game/Hod2.exe"]=expected;record["files"]=owned;InstallerCore.WriteJson(Path.Combine(target,"install.json"),record);
            Reject(delegate{InstallerCore.Uninstall(target,false);},"uninstall rejects injected game ownership before deletion");
            Check(File.Exists(Path.Combine(target,"build/pcvr/ddraw.dll")),"invalid uninstall record deletes nothing");owned.Remove("working/pcvr/game/Hod2.exe");record["files"]=owned;InstallerCore.WriteJson(Path.Combine(target,"install.json"),record);
            File.WriteAllText(Path.Combine(target,"working/pcvr/game/D3DImm.dll"),"user changed DLL");
            InstallerCore.Uninstall(target,false);
            Check(!File.Exists(Path.Combine(target,"working/pcvr/game/ddraw.dll"))&&!File.Exists(Path.Combine(target,"HotD2VR.exe")),"uninstall removes mod and launcher");
            Check(File.Exists(Path.Combine(target,"working/pcvr/game/Hod2.exe"))&&File.ReadAllText(save)=="saved progress"&&File.Exists(settings),"uninstall retains game saves and settings");
            Check(File.ReadAllText(Path.Combine(target,"working/pcvr/game/D3DImm.dll"))=="user changed DLL","uninstall retains modified files");
            Check(Inventory(original).All(p=>before[p.Key]==p.Value)&&Inventory(original).Count==before.Count,"original game intact after complete lifecycle");
            Check(InstallerCore.Quote("C:\\folder with spaces\\")=="\"C:\\folder with spaces\\\\\"","Windows argument quoting handles trailing backslash");
            Console.WriteLine("PASS "+checks+" installer lifecycle checks; only workspace fixtures/shortcuts and an isolated temporary registry key used.");return 0;
        }catch(Exception e){Console.Error.WriteLine(e.ToString());return 1;}
    }
}
