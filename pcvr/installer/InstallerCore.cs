using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace HotD2VRSetup {
public sealed class InstallOptions {
    public string Source, Destination, Disc, Cache;
    public bool Shortcut = true, Register = true;
}
public static class InstallerCore {
    public const string Release = "0.2.1-alpha.23";
    public const string GameHash = "c6b4116788b7f68c56860fb9cc8a94bf984e620907031e4bc3db43623dbe579a";
    public const string DgArchive = "dgVoodoo2_87_5.zip";
    public const string XrArchive = "openxr_loader_windows-1.1.63.zip";
    public const string DgHash = "5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850";
    public const string XrHash = "01c631aeabbfe0879540f77ef833416c532a20746285b494630160c23588b771";
    public const string HelpUrl = "https://github.com/Icedomega13/HouseOfTheDead2VROG/blob/v0.2.1-alpha.23/INSTALL.md";
    public const string GameUrl = "https://www.myabandonware.com/game/the-house-of-the-dead-2-beg";
    public static readonly string[] DataFolders = {"cam","coli","evt","mot","pol","sound","tex"};
    public static readonly string[] PayloadFiles = {"build/pcvr/ddraw.dll","pcvr/run-probe.ps1","pcvr/save-session.ps1","pcvr/graphics-quality.ps1","pcvr/vr-settings.json","LICENSE","THIRD_PARTY.md","INSTALL.md"};
    public static readonly string[] DependencyFiles = {"build/pcvr/openxr_loader.dll","working/pcvr/game/ddraw_backend.dll","working/pcvr/game/D3DImm.dll","working/pcvr/game/dgVoodoo.conf","OPENXR-LICENSE.txt"};
    public static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    public static string Full(string path) {return Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);}
    public static bool Same(string a,string b) {return String.Equals(Full(a),Full(b),StringComparison.OrdinalIgnoreCase);}
    public static bool Inside(string path,string root) {return Full(path).StartsWith(Full(root)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase);}
    public static void NoLinks(string path) {
        string current = Path.GetFullPath(path);
        while (!String.IsNullOrEmpty(current)) {
            if ((Directory.Exists(current)||File.Exists(current)) && (File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0)
                throw new IOException("Linked folders/files are not supported: "+current);
            current = Path.GetDirectoryName(current);
        }
    }
    public static string Under(string root,string relative) {
        string path=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar)));
        if(!Inside(path,root)) throw new IOException("File leaves the installation folder.");
        NoLinks(path); return path;
    }
    public static string Hash(string path) {using(var s=File.OpenRead(path)) using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(s)).Replace("-","").ToLowerInvariant();}
    public static Dictionary<string,object> ReadJson(string path) {return Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));}
    public static void WriteJson(string path,object value) {Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,Json.Serialize(value),new UTF8Encoding(false));}
    public static void Copy(string source,string target) {NoLinks(source);NoLinks(target);Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(source,target,true);File.SetAttributes(target,FileAttributes.Normal);}
    public static List<string> GameFiles(string folder,string expectedHash) {
        folder=Full(folder);NoLinks(folder);
        var files=new List<string>();
        string exe=Under(folder,"Hod2.exe");
        if(!File.Exists(exe)) throw new IOException("Choose your downloaded ZIP or IMG/ISO directly, or an installed PC game folder containing Hod2.exe.");
        if(Hash(exe)!=expectedHash) throw new IOException("This Hod2.exe revision is not supported by this alpha. Use the original Windows PC release; console versions and the remake are incompatible. See setup help.");
        files.Add(exe);
        foreach(string name in new[]{"Hod2.ini","Config.exe"}) {string p=Under(folder,name);if(File.Exists(p))files.Add(p);}
        foreach(string name in DataFolders) {
            string data=Under(folder,name);
            if(!Directory.Exists(data)) throw new IOException("Missing game data folder: "+name+". Select the complete installed game, not just Hod2.exe.");
            var stack=new Stack<string>();stack.Push(data);int before=files.Count;
            while(stack.Count>0) {
                string dir=stack.Pop();NoLinks(dir);
                foreach(string p in Directory.GetFiles(dir)){NoLinks(p);files.Add(p);}
                foreach(string p in Directory.GetDirectories(dir)){NoLinks(p);stack.Push(p);}
            }
            if(files.Count==before) throw new IOException("Game data folder is empty: "+name);
        }
        return files;
    }
    public static void ValidateDisc(string disc) {
        if(String.IsNullOrWhiteSpace(disc))return;
        NoLinks(disc);
        if(!File.Exists(disc)||!String.Equals(Path.GetExtension(disc),".iso",StringComparison.OrdinalIgnoreCase)) throw new IOException("Choose your original game's ISO file, or leave the disc field blank for physical/already-mounted media.");
    }
    public static void Idle() {if(Process.GetProcessesByName("Hod2").Length>0)throw new IOException("Close HOTD2 before installing, updating or removing the mod.");}
    public static Dictionary<string,object> Owned(string root) {
        string path=Under(root,"install.json");
        if(!File.Exists(path))throw new IOException("This folder is not a HotD2VR installer-managed installation.");
        var m=ReadJson(path);
        if(!m.ContainsKey("product")||(string)m["product"]!="HotD2VR"||!m.ContainsKey("root")||!Same((string)m["root"],root))throw new IOException("Installation ownership record does not match this folder.");
        return m;
    }
    public static string RuntimeStatus() {
        using(var h=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,RegistryView.Registry32))
        using(var k=h.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1")) {
            string path=k==null?null:k.GetValue("ActiveRuntime") as string;
            return !String.IsNullOrEmpty(path)&&File.Exists(path)?"32-bit OpenXR runtime found: "+Path.GetFileName(path):"No active 32-bit OpenXR runtime found. Connect your headset and select VDXR in Virtual Desktop Streamer before playing.";
        }
    }
    public static string VerifiedDownload(string cache,string name,string url,string hash,CancellationToken cancel,Action<int,string> progress) {
        NoLinks(cache);Directory.CreateDirectory(cache);string target=Under(cache,name);
        if(File.Exists(target)) {if(Hash(target)!=hash)throw new IOException("Dependency cache checksum failed: "+name+". Delete that cache file and retry.");return target;}
        string partial=Under(cache,name+"."+Guid.NewGuid().ToString("N")+".partial");
        try {
            ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
            var request=(HttpWebRequest)WebRequest.Create(url);request.UserAgent="HotD2VR-Setup/"+Release;request.Timeout=30000;request.ReadWriteTimeout=30000;
            using(cancel.Register(delegate {request.Abort();}))
            using(var response=request.GetResponse()) using(var input=response.GetResponseStream()) using(var output=File.Create(partial)) {
                byte[] buffer=new byte[65536];int count;long bytes=0;
                while((count=input.Read(buffer,0,buffer.Length))>0){cancel.ThrowIfCancellationRequested();bytes+=count;if(bytes>67108864)throw new IOException("Dependency download exceeded its size limit.");output.Write(buffer,0,count);progress(15,"Downloading "+name+" ("+(bytes/1048576)+" MB)");}
            }
            if(Hash(partial)!=hash)throw new IOException("Downloaded dependency checksum failed: "+name+". Nothing has been installed.");
            File.Move(partial,target);return target;
        } finally {if(File.Exists(partial))File.Delete(partial);}
    }
    public static void ExtractEntry(string archive,string entry,string target) {
        using(var z=ZipFile.OpenRead(archive)) {var e=z.GetEntry(entry);if(e==null)throw new IOException("Verified dependency is missing "+entry);Directory.CreateDirectory(Path.GetDirectoryName(target));e.ExtractToFile(target,true);}
    }
    public static void ConfigureBackend(string path) {
        string text=File.ReadAllText(path);
        foreach(var pair in new[]{new[]{"OutputAPI","d3d11_fl11_0"},new[]{"FullScreenMode","false"},new[]{"CaptureMouse","false"}})
            text=Regex.Replace(text,"(?m)^"+pair[0]+@"\s*=.*$",pair[0]+" = "+pair[1]);
        File.WriteAllText(path,text,Encoding.ASCII);
    }
    public static void ExtractPayload(byte[] payload,string root) {
        using(var stream=new MemoryStream(payload)) using(var z=new ZipArchive(stream,ZipArchiveMode.Read)) {
            var m=z.GetEntry("package.json");if(m==null)throw new IOException("Installer payload manifest missing.");
            Dictionary<string,string> hashes;
            using(var reader=new StreamReader(m.Open())) hashes=Json.Deserialize<Dictionary<string,string>>(reader.ReadToEnd());
            if(z.Entries.Count!=PayloadFiles.Length+1||hashes.Count!=PayloadFiles.Length)throw new IOException("Unexpected installer payload inventory.");
            foreach(string path in PayloadFiles) {
                var e=z.GetEntry(path);if(e==null||!hashes.ContainsKey(path))throw new IOException("Installer payload is incomplete.");
                string target=Under(root,path);Directory.CreateDirectory(Path.GetDirectoryName(target));e.ExtractToFile(target);
                if(Hash(target)!=hashes[path])throw new IOException("Installer payload checksum failed: "+path);
            }
        }
    }
    // Only installation-owned staging directories are recursively removed. Reject
    // links at every level before enumeration or deletion.
    public static void RemoveStage(string stage,string parent) {
        if(!Inside(stage,parent)||!Path.GetFileName(stage).StartsWith(".HotD2VR-stage-",StringComparison.Ordinal))throw new IOException("Invalid staging cleanup path.");
        NoLinks(stage);if(!Directory.Exists(stage))return;
        foreach(string p in Directory.GetFiles(stage)){NoLinks(p);File.Delete(p);}
        foreach(string p in Directory.GetDirectories(stage)) RemoveStageTree(p,stage);
        Directory.Delete(stage);
    }
    static void RemoveStageTree(string folder,string owner) {
        if(!Inside(folder,owner))throw new IOException("Cleanup leaves staging folder.");NoLinks(folder);
        foreach(string p in Directory.GetFiles(folder)){NoLinks(p);File.Delete(p);}
        foreach(string p in Directory.GetDirectories(folder))RemoveStageTree(p,owner);
        Directory.Delete(folder);
    }
    public static Mutex OperationMutex(string root) {
        using(var hash=SHA256.Create()){return new Mutex(false,"Local\\HotD2VR-"+BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(Full(root).ToLowerInvariant()))).Replace("-",""));}
    }
    public static string Install(InstallOptions options,byte[] payload,string setupPath,Action<int,string> progress,CancellationToken cancel,string expectedGameHash=GameHash) {
        using(var mutex=OperationMutex(options.Destination)) {
            bool acquired=false;try {try{acquired=mutex.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
                if(!acquired)throw new IOException("Another setup/removal is already using this destination.");
                return InstallLocked(options,payload,setupPath,progress,cancel,expectedGameHash);
            } finally {if(acquired)mutex.ReleaseMutex();}
        }
    }
    static string InstallLocked(InstallOptions options,byte[] payload,string setupPath,Action<int,string> progress,CancellationToken cancel,string expectedGameHash) {
        Idle();string root=Full(options.Destination),source=Full(options.Source);NoLinks(root);NoLinks(source);ValidateDisc(options.Disc);
        string windows=Environment.GetFolderPath(Environment.SpecialFolder.Windows);
        if(root.Length<4||Same(root,Path.GetPathRoot(root))||Same(root,windows)||Inside(root,windows)||Same(root,Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)))throw new IOException("Choose a dedicated HotD2VR destination folder.");
        bool update=File.Exists(Under(root,"install.json"));
        if(update)Owned(root);
        else if(Directory.Exists(root)&&Directory.EnumerateFileSystemEntries(root).Any())throw new IOException("Choose an empty destination folder. Existing unrelated files are never overwritten.");
        string game=Under(root,"working/pcvr/game");
        if((Same(root,source)||Inside(root,source)||Inside(source,root))&&!(update&&Same(source,game)))throw new IOException("The mod destination must be separate from your original game folder.");
        bool import=!update&&File.Exists(source);
        progress(2,"Checking the original game files...");var gameFiles=import?null:GameFiles(update?game:source,expectedGameHash);cancel.ThrowIfCancellationRequested();
        long size=update?104857600:import?3221225472:gameFiles.Sum(p=>new FileInfo(p).Length)+104857600;
        if(new DriveInfo(Path.GetPathRoot(root)).AvailableFreeSpace<size)throw new IOException("Not enough free disk space for the separate game copy.");
        string parent=Path.GetDirectoryName(root);Directory.CreateDirectory(parent);
        string stage=Under(parent,".HotD2VR-stage-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);
        var changed=new List<string>();string backup=null;bool committed=false;
        try {
            if(import)DiscImport.Import(source,stage,expectedGameHash,cancel,progress);
            ExtractPayload(payload,stage);
            string dg=VerifiedDownload(options.Cache,DgArchive,"https://github.com/dege-diosg/dgVoodoo2/releases/download/v2.87.5/"+DgArchive,DgHash,cancel,progress);
            string xr=VerifiedDownload(options.Cache,XrArchive,"https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/release-1.1.63/"+XrArchive,XrHash,cancel,progress);
            progress(30,"Preparing verified graphics and OpenXR dependencies...");
            ExtractEntry(dg,"MS/x86/DDraw.dll",Under(stage,"working/pcvr/game/ddraw_backend.dll"));
            ExtractEntry(dg,"MS/x86/D3DImm.dll",Under(stage,"working/pcvr/game/D3DImm.dll"));
            ExtractEntry(dg,"dgVoodoo.conf",Under(stage,"working/pcvr/game/dgVoodoo.conf"));ConfigureBackend(Under(stage,"working/pcvr/game/dgVoodoo.conf"));
            ExtractEntry(xr,"Win32/bin/openxr_loader.dll",Under(stage,"build/pcvr/openxr_loader.dll"));
            ExtractEntry(xr,"share/doc/openxr/LICENSE",Under(stage,"OPENXR-LICENSE.txt"));
            Copy(Under(stage,"build/pcvr/ddraw.dll"),Under(stage,"working/pcvr/game/ddraw.dll"));
            Copy(Under(stage,"build/pcvr/openxr_loader.dll"),Under(stage,"working/pcvr/game/openxr_loader.dll"));
            Copy(setupPath,Under(stage,"HotD2VR.exe"));
            if(!update&&!import) {
                int done=0;foreach(string file in gameFiles) {cancel.ThrowIfCancellationRequested();string relative=file.Substring(source.Length+1);Copy(file,Under(stage,"working/pcvr/game/"+relative));progress(35+(++done*40/gameFiles.Count),"Copying your game ("+done+" / "+gameFiles.Count+")...");}
            } else if(update) {
                string settings=Under(root,"pcvr/vr-settings.json");if(File.Exists(settings))Copy(settings,Under(stage,"pcvr/vr-settings.json"));
            }
            // Verify the playable executable after copying, before changing destination.
            if(!update&&Hash(Under(stage,"working/pcvr/game/Hod2.exe"))!=expectedGameHash)throw new IOException("Copied game executable verification failed.");
            cancel.ThrowIfCancellationRequested();Idle();
            var modPaths=new List<string>(PayloadFiles);modPaths.AddRange(DependencyFiles);modPaths.AddRange(new[]{"working/pcvr/game/ddraw.dll","working/pcvr/game/openxr_loader.dll","HotD2VR.exe"});
            var owned=new Dictionary<string,string>();foreach(string path in modPaths)owned[path]=Hash(Under(stage,path));
            string disc=import?Under(root,"working/intake/windows-data.iso"):String.IsNullOrWhiteSpace(options.Disc)?"":Full(options.Disc);
            if(update&&String.IsNullOrWhiteSpace(options.Disc)){var previous=Owned(root);if(previous.ContainsKey("disc"))disc=previous["disc"] as string??"";}
            WriteJson(Under(stage,"install.json"),new{product="HotD2VR",root=root,release=Release,probe=23,game_sha256=expectedGameHash,source=source,disc=disc,files=owned});
            if(!update) {if(Directory.Exists(root))Directory.Delete(root);Directory.Move(stage,root);}
            else {
                backup=Under(root,"backups/"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8));Directory.CreateDirectory(backup);
                // Preflight and back up all replacements before any update write.
                foreach(string path in modPaths.Concat(new[]{"install.json"})) {string target=Under(root,path);if(File.Exists(target)){using(var f=new FileStream(target,FileMode.Open,FileAccess.ReadWrite,FileShare.None)){} Copy(target,Under(backup,path));}}
                foreach(string path in modPaths.Concat(new[]{"install.json"})) {changed.Add(path);Copy(Under(stage,path),Under(root,path));}
            }
            committed=true;progress(92,"Creating launcher and removal entry...");
            // Integration failure must not invalidate a fully committed installation.
            string note="";
            try {if(options.Shortcut)CreateShortcut(root);if(options.Register)Register(root);} catch(Exception e){note="\nThe mod installed, but shortcut/Apps integration failed: "+e.Message+"\nRun HotD2VR.exe /play from the installation folder.";}
            progress(100,"Installed HotD2VR "+Release+". "+RuntimeStatus());return root+note;
        } catch {
            if(!committed&&backup!=null)foreach(string path in changed.AsEnumerable().Reverse()){string old=Under(backup,path),target=Under(root,path);if(File.Exists(old))Copy(old,target);else if(File.Exists(target))File.Delete(target);}
            throw;
        } finally {RemoveStage(stage,parent);}
    }
    public static string ShortcutPath() {return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),"Play HotD2VR.lnk");}
    public const string UninstallKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\HotD2VR";
    public static void CreateShortcut(string root,string overridePath=null) {
        string path=overridePath??ShortcutPath();NoLinks(path);
        Type t=Type.GetTypeFromProgID("WScript.Shell");object shell=Activator.CreateInstance(t);object link=t.InvokeMember("CreateShortcut",System.Reflection.BindingFlags.InvokeMethod,null,shell,new object[]{path});
        string existing=(string)link.GetType().InvokeMember("TargetPath",System.Reflection.BindingFlags.GetProperty,null,link,null);
        if(!String.IsNullOrEmpty(existing)&&!Same(existing,Under(root,"HotD2VR.exe"))) {
            System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
            throw new IOException("A Play HotD2VR shortcut already points to another installation; it was retained.");
        }
        Type lt=link.GetType();lt.InvokeMember("TargetPath",System.Reflection.BindingFlags.SetProperty,null,link,new object[]{Under(root,"HotD2VR.exe")});
        lt.InvokeMember("Arguments",System.Reflection.BindingFlags.SetProperty,null,link,new object[]{"/play"});lt.InvokeMember("WorkingDirectory",System.Reflection.BindingFlags.SetProperty,null,link,new object[]{root});
        lt.InvokeMember("Description",System.Reflection.BindingFlags.SetProperty,null,link,new object[]{"The House of the Dead 2 PCVR"});lt.InvokeMember("Save",System.Reflection.BindingFlags.InvokeMethod,null,link,null);
        System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
    }
    public static void Register(string root,string keyPath=UninstallKey) {
        using(var key=Registry.CurrentUser.CreateSubKey(keyPath)) {
            string existing=key.GetValue("InstallLocation") as string;
            if(!String.IsNullOrEmpty(existing)&&!Same(existing,root))throw new IOException("Another HotD2VR installation is registered; its removal entry was retained.");
            key.SetValue("DisplayName","HotD2VR Alpha");key.SetValue("DisplayVersion",Release);key.SetValue("Publisher","HotD2VR contributors");key.SetValue("InstallLocation",root);
            key.SetValue("UninstallString",Quote(Under(root,"HotD2VR.exe"))+" /uninstall "+Quote(root));key.SetValue("URLInfoAbout","https://github.com/Icedomega13/HouseOfTheDead2VROG");key.SetValue("NoModify",1);key.SetValue("NoRepair",1);
        }
    }
    public static string Quote(string value) {
        var b=new StringBuilder("\"");int slashes=0;
        foreach(char c in value){if(c=='\\'){++slashes;continue;}b.Append('\\',c=='"'?slashes*2+1:slashes);b.Append(c);slashes=0;}
        b.Append('\\',slashes*2);b.Append('"');return b.ToString();
    }
    public static string Uninstall(string root,bool integration) {
        using(var mutex=OperationMutex(root)) {
            bool acquired=false;try{try{acquired=mutex.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
                if(!acquired)throw new IOException("Another setup/removal is already using this destination.");return UninstallLocked(root,integration);
            }finally{if(acquired)mutex.ReleaseMutex();}
        }
    }
    static string UninstallLocked(string root,bool integration) {
        root=Full(root);Idle();var record=Owned(root);var files=Json.ConvertToType<Dictionary<string,string>>(record["files"]);
        var allowed=new HashSet<string>(PayloadFiles.Concat(DependencyFiles).Concat(new[]{"working/pcvr/game/ddraw.dll","working/pcvr/game/openxr_loader.dll","HotD2VR.exe"}));
        foreach(var pair in files){if(!allowed.Contains(pair.Key))throw new IOException("Unexpected file in ownership record; removal stopped.");Under(root,pair.Key);}
        var preserved=new List<string>();
        foreach(var pair in files) {
            if(!allowed.Contains(pair.Key))throw new IOException("Unexpected file in ownership record; removal stopped.");
            string path=Under(root,pair.Key);if(!File.Exists(path))continue;
            if(pair.Key=="pcvr/vr-settings.json"||pair.Key=="working/pcvr/game/dgVoodoo.conf"||Hash(path)!=pair.Value){preserved.Add(pair.Key);continue;}
            File.Delete(path);
        }
        if(integration)RemoveIntegration(root);
        File.Delete(Under(root,"install.json"));
        return "Mod removed. Your original game, copied game/saves, settings and backups remain.\n\nRetained folder: "+root+(preserved.Count>0?"\nSettings or changed files retained: "+String.Join(", ",preserved):"");
    }
    public static void RemoveIntegration(string root,string keyPath=UninstallKey,string overridePath=null) {
            bool remove=false;
            using(var key=Registry.CurrentUser.OpenSubKey(keyPath)) {
                string location=key==null?null:key.GetValue("InstallLocation") as string;
                remove=!String.IsNullOrEmpty(location)&&Same(location,root);
            }
            if(remove)Registry.CurrentUser.DeleteSubKeyTree(keyPath,false);
            string shortcut=overridePath??ShortcutPath();NoLinks(shortcut);if(File.Exists(shortcut)) {
                Type t=Type.GetTypeFromProgID("WScript.Shell");object shell=Activator.CreateInstance(t);object link=t.InvokeMember("CreateShortcut",System.Reflection.BindingFlags.InvokeMethod,null,shell,new object[]{shortcut});
                string target=(string)link.GetType().InvokeMember("TargetPath",System.Reflection.BindingFlags.GetProperty,null,link,null);
                System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
                if(Same(target,Under(root,"HotD2VR.exe")))File.Delete(shortcut);
            }
    }
}
}
