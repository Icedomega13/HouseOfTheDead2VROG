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
                    var entry=zip.CreateEntry(name);string text=name=="pcvr/vr-settings.json"?"{\"EyeSize\":1200,\"AimingCursor\":false,\"DualWield\":true,\"IndependentMagazines\":true,\"AmmoGauges\":true,\"HealthGauge\":true}":version+" "+name;
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
    static void Put(byte[] bytes,int offset,uint value){Buffer.BlockCopy(BitConverter.GetBytes(value),0,bytes,offset,4);}
    static int DiscRecord(byte[] bytes,int offset,string name,uint sector,uint size,byte flags=0) {
        byte[] text=Encoding.ASCII.GetBytes(name);int length=33+text.Length;if(length%2!=0)length++;
        bytes[offset]=(byte)length;Put(bytes,offset+2,sector);Put(bytes,offset+10,size);bytes[offset+25]=flags;bytes[offset+32]=(byte)text.Length;Buffer.BlockCopy(text,0,bytes,offset+33,text.Length);return length;
    }
    static byte[] DiscFixture() {
        byte[] iso=new byte[24*2048];int pvd=16*2048;iso[pvd]=1;Encoding.ASCII.GetBytes("CD001").CopyTo(iso,pvd+1);iso[pvd+6]=1;
        DiscRecord(iso,pvd+156,"\0",20,2048,2);int pos=20*2048;
        pos+=DiscRecord(iso,pos,"DATA.CAB;1",21,1);pos+=DiscRecord(iso,pos,"THE HOUSE OF THE DEAD 2.MSI;1",22,1);DiscRecord(iso,pos,"HOD2.EXE;1",23,1);
        iso[21*2048]=10;iso[22*2048]=20;iso[23*2048]=30;return iso;
    }
    static void DiscChecks(string home,string cache,string setup) {
        string area=Path.Combine(home,"Disc import fixtures");Directory.CreateDirectory(area);byte[] iso=DiscFixture();
        string input=Path.Combine(area,"source.iso");File.WriteAllBytes(input,iso);
        string output=Path.Combine(area,"prepared.iso");DiscImport.PrepareIso(input,output,CancellationToken.None,Progress);
        Check(InstallerCore.Hash(input)==InstallerCore.Hash(output),"ISO imported without changing its bytes");
        string raw=Path.Combine(area,"source.img");using(var stream=File.Create(raw))for(int i=0;i<iso.Length/2048;i++) {byte[] sector=new byte[2352];for(int j=1;j<11;j++)sector[j]=255;sector[15]=1;Buffer.BlockCopy(iso,i*2048,sector,16,2048);stream.Write(sector,0,sector.Length);}
        string rawOutput=Path.Combine(area,"raw.iso");DiscImport.PrepareIso(raw,rawOutput,CancellationToken.None,Progress);
        Check(InstallerCore.Hash(input)==InstallerCore.Hash(rawOutput),"MODE1/2352 IMG data track converts exactly to ISO");
        string archive=Path.Combine(area,"download.zip");using(var zip=ZipFile.Open(archive,ZipArchiveMode.Create)){zip.CreateEntryFromFile(raw,"Disc/source.img");using(var entry=zip.CreateEntry("Disc/source.sub").Open())entry.WriteByte(77);}
        string zipped=Path.Combine(area,"zip.iso");DiscImport.PrepareIso(archive,zipped,CancellationToken.None,Progress);
        Check(InstallerCore.Hash(input)==InstallerCore.Hash(zipped),"ZIP directly converts only its image, without extracting SUB");
        Check(!Directory.Exists(Path.Combine(area,"Disc")),"archive paths are never extracted as folders");
        string extracted=Path.Combine(area,"Root disc files");Directory.CreateDirectory(extracted);DiscImport.ReadDiscFiles(output,extracted,CancellationToken.None);
        Check(Directory.GetFiles(extracted).Length==3&&File.ReadAllBytes(Path.Combine(extracted,"HOD2.EXE"))[0]==30,"bounded ISO reader extracts only three expected root files");
        Reject(delegate{DiscImport.ReadLayout(Path.Combine(extracted,"THE HOUSE OF THE DEAD 2.MSI"));},"invalid MSI metadata rejected without executing it");
        foreach(string name in new[]{"..","../escape","C:bad","CON","com1.txt","LPT0","trailing.","trailing "})Reject(delegate{DiscImport.SafeName(name);},"unsafe/reserved disc filename rejected: "+name);
        string malicious=Path.Combine(area,"unsafe.zip");using(var zip=ZipFile.Open(malicious,ZipArchiveMode.Create)){zip.CreateEntry("../escape.img");}
        Reject(delegate{DiscImport.PrepareIso(malicious,Path.Combine(area,"unsafe.iso"),CancellationToken.None,Progress);},"ZIP path traversal rejected before output");
        string ambiguous=Path.Combine(area,"ambiguous.zip");using(var zip=ZipFile.Open(ambiguous,ZipArchiveMode.Create)){zip.CreateEntry("one.img");zip.CreateEntry("two.iso");}
        Reject(delegate{DiscImport.PrepareIso(ambiguous,Path.Combine(area,"ambiguous.iso"),CancellationToken.None,Progress);},"multiple disc images require an explicit selection");
        string empty=Path.Combine(area,"empty.zip");using(var zip=ZipFile.Open(empty,ZipArchiveMode.Create)){zip.CreateEntry("readme.txt");}
        Reject(delegate{DiscImport.PrepareIso(empty,Path.Combine(area,"empty.iso"),CancellationToken.None,Progress);},"ZIP with no supported image rejected");
        using(var file=new FileStream(raw,FileMode.Open,FileAccess.Write)){file.Position=2352+15;file.WriteByte(2);}
        Reject(delegate{DiscImport.PrepareIso(raw,Path.Combine(area,"wrong-mode.iso"),CancellationToken.None,Progress);},"mixed/non-MODE1 raw sectors rejected");
        byte[] damaged=(byte[])iso.Clone();damaged[16*2048+1]=0;File.WriteAllBytes(Path.Combine(area,"damaged.iso"),damaged);
        Reject(delegate{DiscImport.PrepareIso(Path.Combine(area,"damaged.iso"),Path.Combine(area,"bad-pvd.iso"),CancellationToken.None,Progress);},"missing ISO9660 data track rejected");
        damaged=(byte[])iso.Clone();Put(damaged,16*2048+158,500);File.WriteAllBytes(Path.Combine(area,"bad-offset.iso"),damaged);
        Reject(delegate{DiscImport.ReadDiscFiles(Path.Combine(area,"bad-offset.iso"),extracted,CancellationToken.None);},"ISO extent outside image rejected");
        damaged=(byte[])iso.Clone();damaged[20*2048+25]=128;File.WriteAllBytes(Path.Combine(area,"multi-extent.iso"),damaged);
        Reject(delegate{DiscImport.ReadDiscFiles(Path.Combine(area,"multi-extent.iso"),extracted,CancellationToken.None);},"multi-extent game files rejected");
        damaged=(byte[])iso.Clone();damaged[20*2048]=1;File.WriteAllBytes(Path.Combine(area,"bad-record.iso"),damaged);
        Reject(delegate{DiscImport.ReadDiscFiles(Path.Combine(area,"bad-record.iso"),extracted,CancellationToken.None);},"malformed ISO directory rejected");
        string huge=Path.Combine(area,"oversized.iso");using(var file=File.Create(huge))file.SetLength(1073741824L+2048);
        Reject(delegate{DiscImport.PrepareIso(huge,Path.Combine(area,"oversized-output.iso"),CancellationToken.None,Progress);},"oversized image rejected before reading it");File.Delete(huge);
        var cancellation=new CancellationTokenSource();bool cancelled=false;
        try{DiscImport.PrepareIso(input,Path.Combine(area,"cancelled.iso"),cancellation.Token,delegate(int value,string text){cancellation.Cancel();});}catch(OperationCanceledException){cancelled=true;}
        Check(cancelled,"image conversion observes cancellation between sectors");
        string target=Path.Combine(area,"Rejected installation");
        Reject(delegate{InstallerCore.Install(Options(malicious,target,cache),Package("fixture"),setup,Progress,CancellationToken.None);},"invalid ZIP commits no installation");
        Check(!Directory.Exists(target)&&Directory.GetDirectories(area,".HotD2VR-stage-*").Length==0,"failed disc import cleans its owned staging files");
        string one=Path.Combine(area,"one.bin"),cab=Path.Combine(area,"fixture.cab");File.WriteAllText(one,"Cabinet fixture, not game data");
        using(var process=System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"makecab.exe"),InstallerCore.Quote(one)+" "+InstallerCore.Quote(cab)){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true})){process.StandardOutput.ReadToEnd();process.StandardError.ReadToEnd();process.WaitForExit();Check(process.ExitCode==0,"Windows builds a tiny cabinet fixture");}
        var layout=new List<DiscImport.GameEntry>{new DiscImport.GameEntry{Id="one.bin",Relative="cam/one.bin",Size=new FileInfo(one).Length}};
        string cabGame=Path.Combine(area,"Cabinet game");DiscImport.ExtractCabinet(cab,cabGame,layout,CancellationToken.None,Progress);
        Check(InstallerCore.Hash(one)==InstallerCore.Hash(Path.Combine(cabGame,"cam/one.bin")),"actual Windows cabinet callback extracts an exact mapped file");
        layout[0].Size++;
        Reject(delegate{DiscImport.ExtractCabinet(cab,Path.Combine(area,"Wrong size"),layout,CancellationToken.None,Progress);},"native cabinet rejects MSI/file size mismatch");layout[0].Size--;
        layout[0].Id="unknown.bin";Reject(delegate{DiscImport.ExtractCabinet(cab,Path.Combine(area,"Unknown file"),layout,CancellationToken.None,Progress);},"unmapped cabinet entry rejected");layout[0].Id="one.bin";
        var stop=new CancellationTokenSource();stop.Cancel();cancelled=false;
        try{DiscImport.ExtractCabinet(cab,Path.Combine(area,"Cancelled cabinet"),layout,stop.Token,Progress);}catch(OperationCanceledException){cancelled=true;}
        Check(cancelled,"native cabinet cancellation stays within managed error handling");
        Check(InstallerCore.Hash(input)==InstallerCore.Hash(output),"selected source image remains unchanged after failure tests");
    }
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
            DiscChecks(home,cache,setup);
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
            string settings=Path.Combine(target,"pcvr/vr-settings.json"),save=Path.Combine(target,"working/pcvr/game/player-save.dat");
            var fresh=InstallerCore.ReadJson(settings);Check((bool)fresh["DualWield"]&&(bool)fresh["IndependentMagazines"]&&(bool)fresh["HealthGauge"]&&!(bool)fresh["AimingCursor"],"new install enables dual status HUD and starts aiming dots off");
            string oldSettings="{\"EyeSize\":800,\"AimingCursor\":true,\"HealthGauge\":false,\"CustomPreference\":\"keep\"}";
            File.WriteAllText(settings,oldSettings);File.WriteAllText(save,"saved progress");
            string old=InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"));
            var options=Options(Path.Combine(target,"working/pcvr/game"),target,cache);
            var withDisc=InstallerCore.Owned(target);withDisc["disc"]=Path.Combine(target,"working/intake/windows-data.iso");InstallerCore.WriteJson(Path.Combine(target,"install.json"),withDisc);
            InstallerCore.Install(options,Package("v2"),setup,Progress,CancellationToken.None,expected);
            Check((string)InstallerCore.Owned(target)["disc"]==(string)withDisc["disc"],"update preserves the automatic imported-disc path");
            Check(InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))!=old,"update replaces mod DLL");
            Check(File.ReadAllText(settings).Contains("800")&&File.ReadAllText(save)=="saved progress","update preserves preferences and saves");
            var upgraded=InstallerCore.ReadJson(settings);
            Check((bool)upgraded["DualWield"]&&(bool)upgraded["IndependentMagazines"]&&(bool)upgraded["AmmoGauges"],"upgrade adds missing feature defaults");
            Check((bool)upgraded["AimingCursor"]&&!(bool)upgraded["HealthGauge"]&&(string)upgraded["CustomPreference"]=="keep","upgrade preserves explicit true/false and unknown preferences");
            string[] backups=Directory.GetDirectories(Path.Combine(target,"backups"));Check(backups.Length==1&&InstallerCore.Hash(Path.Combine(backups[0],"build/pcvr/ddraw.dll"))==old,"previous mod preserved before update");
            Check(File.ReadAllText(Path.Combine(backups[0],"pcvr/vr-settings.json"))==oldSettings,"update backs up original preferences byte-for-byte");
            Check(Directory.GetDirectories(home,".HotD2VR-stage-*").Length==0,"successful update staging cleaned");
            string current=InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"));
            string validSettings=File.ReadAllText(settings);File.WriteAllText(settings,"invalid JSON");
            Reject(delegate{InstallerCore.Install(options,Package("v3"),setup,Progress,CancellationToken.None,expected);},"invalid saved settings abort update before replacement");
            Check(File.ReadAllText(settings)=="invalid JSON"&&InstallerCore.Hash(Path.Combine(target,"build/pcvr/ddraw.dll"))==current,"invalid-settings update leaves old installation intact");File.WriteAllText(settings,validSettings);
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
