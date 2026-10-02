using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace HotD2VRSetup {
// Reads media as data only. Never starts autorun, setup.exe or MSI actions.
public static class DiscImport {
    const long MaxImage = 1073741824, MaxGame = 2147483648;
    public sealed class GameEntry {public string Id, Relative;public long Size;}
    sealed class Extent {public long Offset, Size;}
    public static void Import(string source,string stage,string expectedHash,CancellationToken cancel,Action<int,string> progress) {
        string scratch=InstallerCore.Under(stage,".HotD2VR-stage-disc-import");Directory.CreateDirectory(scratch);
        string iso=InstallerCore.Under(stage,"working/intake/windows-data.iso");Directory.CreateDirectory(Path.GetDirectoryName(iso));
        try {
            progress(3,"Preparing your disc image (no original game installation needed)...");
            PrepareIso(source,iso,cancel,progress);
            ReadDiscFiles(iso,scratch,cancel);
            if(InstallerCore.Hash(Path.Combine(scratch,"HOD2.EXE"))!=expectedHash)throw new IOException("This disc contains an unsupported Hod2.exe revision. Choose the original Windows PC disc download.");
            var entries=ReadLayout(Path.Combine(scratch,"THE HOUSE OF THE DEAD 2.MSI"));
            if(new DriveInfo(Path.GetPathRoot(stage)).AvailableFreeSpace<entries.Sum(e=>e.Size)+104857600)throw new IOException("Not enough free disk space to extract the game. Choose a drive with more free space.");
            ExtractCabinet(Path.Combine(scratch,"DATA.CAB"),InstallerCore.Under(stage,"working/pcvr/game"),entries,cancel,progress);
            InstallerCore.GameFiles(InstallerCore.Under(stage,"working/pcvr/game"),expectedHash);
        } finally {InstallerCore.RemoveStage(scratch,stage);}
    }
    public static void PrepareIso(string source,string target,CancellationToken cancel,Action<int,string> progress) {
        InstallerCore.NoLinks(source);cancel.ThrowIfCancellationRequested();string ext=Path.GetExtension(source).ToLowerInvariant();
        if(ext==".zip") {
            using(var zip=ZipFile.OpenRead(source)) {
                if(zip.Entries.Count>10000)throw new IOException("The download contains too many archive entries.");
                var images=new List<ZipArchiveEntry>();
                foreach(var e in zip.Entries) {
                    string path=e.FullName.Replace('\\','/');
                    if(path.StartsWith("/")||path.Contains(":")||path.Split('/').Any(p=>p==".."))throw new IOException("The ZIP contains an unsafe path. Nothing has been installed.");
                    string type=Path.GetExtension(path).ToLowerInvariant();if(type==".img"||type==".iso")images.Add(e);
                }
                if(images.Count!=1)throw new IOException("Choose a ZIP containing exactly one Windows PC game IMG or ISO. For multiple discs, extract and select the correct image directly.");
                var image=images[0];using(var input=image.Open())Normalize(input,image.Length,Path.GetExtension(image.FullName).ToLowerInvariant(),target,cancel,progress);
            }
        } else if(ext==".img"||ext==".iso") {using(var input=File.OpenRead(source))Normalize(input,input.Length,ext,target,cancel,progress);}
        else throw new IOException("Choose the downloaded ZIP, its IMG/ISO disc image, or an installed original game folder.");
    }
    static void Exact(Stream input,byte[] bytes,int count) {
        int done=0,n;while(done<count&&(n=input.Read(bytes,done,count-done))>0)done+=n;
        if(done!=count)throw new IOException("The disc image is truncated. Download it again.");
    }
    static void Normalize(Stream input,long length,string ext,string target,CancellationToken cancel,Action<int,string> progress) {
        if(length<17*2048||length>MaxImage)throw new IOException("The disc image size is unsupported (maximum 1 GB).");
        byte[] first=new byte[2048];Exact(input,first,first.Length);
        bool raw=first[0]==0&&first[1]==255&&first[11]==0&&first[15]==1;
        int sector=raw?2352:2048;if(length%sector!=0||(!raw&&ext==".img"))throw new IOException("This image is not a supported ISO or MODE1/2352 Windows PC disc.");
        byte[] block=new byte[sector];Buffer.BlockCopy(first,0,block,0,first.Length);
        long sectors=length/sector;
        Directory.CreateDirectory(Path.GetDirectoryName(target));
        using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write,FileShare.None,131072)) {
            for(long i=0;i<sectors;i++) {
                cancel.ThrowIfCancellationRequested();
                if(i==0&&raw) {byte[] tail=new byte[304];Exact(input,tail,tail.Length);Buffer.BlockCopy(tail,0,block,2048,304);}
                else if(i!=0)Exact(input,block,sector);
                if(raw) {if(block[0]!=0||block[11]!=0||block[15]!=1)throw new IOException("Unsupported or damaged raw disc sector.");for(int j=1;j<11;j++)if(block[j]!=255)throw new IOException("Damaged raw disc sector sync.");}
                int offset=raw?16:0;
                if(i==16&&(block[offset]!=1||Encoding.ASCII.GetString(block,offset+1,5)!="CD001"||block[offset+6]!=1))throw new IOException("The image does not contain a supported ISO9660 data track.");
                output.Write(block,offset,2048);
                if(i%4096==0)progress(3+(int)(i*12/sectors),"Preparing disc ("+(i*100/sectors)+"%)...");
            }
            if(input.ReadByte()!=-1)throw new IOException("The image is larger than its archive metadata.");
        }
    }
    static uint Little(byte[] b,int offset){return BitConverter.ToUInt32(b,offset);}
    static Extent Location(byte[] b,int start,long length) {
        long offset=(long)Little(b,start+2)*2048,size=Little(b,start+10);
        if(offset<0||size<0||offset>length-size)throw new IOException("A disc file points outside the image.");
        return new Extent{Offset=offset,Size=size};
    }
    public static void ReadDiscFiles(string iso,string target,CancellationToken cancel) {
        using(var input=File.OpenRead(iso)) {
            byte[] pvd=new byte[2048];input.Position=16*2048;Exact(input,pvd,2048);
            if(pvd[0]!=1||Encoding.ASCII.GetString(pvd,1,5)!="CD001"||pvd[6]!=1||pvd[156]<34)throw new IOException("Invalid ISO9660 volume descriptor.");
            var root=Location(pvd,156,input.Length);if(root.Size<34||root.Size>1048576)throw new IOException("Unsupported disc directory.");
            byte[] dir=new byte[(int)root.Size];input.Position=root.Offset;Exact(input,dir,dir.Length);
            var entries=new Dictionary<string,Extent>(StringComparer.OrdinalIgnoreCase);
            for(int pos=0;pos<dir.Length;) {
                int n=dir[pos];if(n==0){pos=((pos/2048)+1)*2048;continue;}
                if(n<34||pos+n>dir.Length||pos%2048+n>2048||dir[pos+32]+33>n)throw new IOException("Damaged ISO directory record.");
                string name=Encoding.ASCII.GetString(dir,pos+33,dir[pos+32]).Split(';')[0];
                if(name=="DATA.CAB"||name=="THE HOUSE OF THE DEAD 2.MSI"||name=="HOD2.EXE") {
                    if((dir[pos+25]&130)!=0||entries.ContainsKey(name))throw new IOException("Unsupported multi-part/duplicate disc file.");
                    var location=Location(dir,pos,input.Length);
                    long limit=name=="DATA.CAB"?734003200:16777216;
                    if(location.Size==0||location.Size>limit)throw new IOException("Disc installer file size exceeds its limit.");entries.Add(name,location);
                }
                pos+=n;
            }
            if(entries.Count!=3)throw new IOException("This disc layout is not supported. Choose the original Windows PC disc containing HOD2.EXE, DATA.CAB and THE HOUSE OF THE DEAD 2.MSI, or use an installed game folder.");
            foreach(var entry in entries) {
                input.Position=entry.Value.Offset;byte[] buffer=new byte[131072];long remaining=entry.Value.Size;
                using(var output=File.Create(InstallerCore.Under(target,entry.Key)))while(remaining>0){cancel.ThrowIfCancellationRequested();int n=(int)Math.Min(buffer.Length,remaining);Exact(input,buffer,n);output.Write(buffer,0,n);remaining-=n;}
            }
        }
    }
    [DllImport("msi.dll",CharSet=CharSet.Unicode)]static extern uint MsiOpenDatabaseW(string path,IntPtr mode,out uint database);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)]static extern uint MsiDatabaseOpenViewW(uint database,string query,out uint view);
    [DllImport("msi.dll")]static extern uint MsiViewExecute(uint view,uint record);
    [DllImport("msi.dll")]static extern uint MsiViewFetch(uint view,out uint record);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)]static extern uint MsiRecordGetStringW(uint record,uint field,StringBuilder value,ref uint length);
    [DllImport("msi.dll")]static extern uint MsiCloseHandle(uint handle);
    static void MsiCheck(uint result){if(result!=0)throw new IOException("Cannot read the disc's installer metadata (Windows error "+result+").");}
    static List<string[]> Rows(uint db,string query,int columns) {
        uint view;MsiCheck(MsiDatabaseOpenViewW(db,query,out view));var rows=new List<string[]>();
        try {MsiCheck(MsiViewExecute(view,0));uint record,result;
            while((result=MsiViewFetch(view,out record))==0) {
                try {if(rows.Count>=10000)throw new IOException("Too many installer metadata rows.");var row=new string[columns];
                    for(uint i=0;i<columns;i++){uint size=4096;var value=new StringBuilder(4097);MsiCheck(MsiRecordGetStringW(record,i+1,value,ref size));row[i]=value.ToString();}rows.Add(row);
                }finally{MsiCloseHandle(record);}
            }if(result!=259)MsiCheck(result);
        }finally{MsiCloseHandle(view);}return rows;
    }
    public static void SafeName(string name) {
        if(String.IsNullOrEmpty(name)||name=="."||name==".."||name.Length>128||name.IndexOfAny(Path.GetInvalidFileNameChars())>=0||name.EndsWith(".")||name.EndsWith(" "))throw new IOException("Unsafe filename in the disc metadata.");
        string stem=name.Split('.')[0].ToUpperInvariant();
        if(new[]{"CON","PRN","AUX","NUL","CLOCK$"}.Contains(stem)||(stem.Length==4&&(stem.StartsWith("COM")||stem.StartsWith("LPT"))&&stem[3]>='0'&&stem[3]<='9'))throw new IOException("Reserved filename in the disc metadata.");
    }
    static string Resolve(string id,Dictionary<string,string[]> dirs,HashSet<string> seen) {
        if(id=="INSTALLDIR")return "";
        if(seen.Count>32||!seen.Add(id)||!dirs.ContainsKey(id))throw new IOException("The disc contains an invalid game directory tree.");
        var d=dirs[id];string parent=Resolve(d[1],dirs,seen),name=d[2].Split(':')[0].Split('|').Last();
        if(name==".")return parent;SafeName(name);return String.IsNullOrEmpty(parent)?name:parent+"/"+name;
    }
    public static List<GameEntry> ReadLayout(string msi) {
        uint db;MsiCheck(MsiOpenDatabaseW(msi,IntPtr.Zero,out db)); // MSIDBOPEN_READONLY
        try {
            var dirs=Rows(db,"SELECT `Directory`, `Directory_Parent`, `DefaultDir` FROM `Directory`",3).ToDictionary(r=>r[0]);
            var components=Rows(db,"SELECT `Component`, `Directory_` FROM `Component`",2).ToDictionary(r=>r[0],r=>r[1]);
            var list=new List<GameEntry>();var ids=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var paths=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long total=0;
            foreach(var row in Rows(db,"SELECT `File`, `Component_`, `FileName`, `FileSize` FROM `File`",4)) {
                SafeName(row[0]);string directory;if(!components.TryGetValue(row[1],out directory))throw new IOException("Missing game component metadata.");
                directory=Resolve(directory,dirs,new HashSet<string>());string name=row[2].Split('|').Last();SafeName(name);
                string relative=String.IsNullOrEmpty(directory)?name:directory+"/"+name;long size;
                if(!Int64.TryParse(row[3],out size)||size<0||size>268435456||(total+=size)>MaxGame)throw new IOException("Unsupported game file sizes.");
                bool allowed=String.IsNullOrEmpty(directory)?new[]{"Hod2.exe","Hod2.ini","Config.exe"}.Contains(name,StringComparer.OrdinalIgnoreCase):InstallerCore.DataFolders.Contains(directory.Split('/')[0],StringComparer.OrdinalIgnoreCase);
                if(!allowed||!ids.Add(row[0])||!paths.Add(relative))throw new IOException("Unsafe or duplicate game file mapping.");
                list.Add(new GameEntry{Id=row[0],Relative=relative,Size=size});
            }
            if(list.Count<10)throw new IOException("The game file manifest is incomplete.");return list;
        }catch(ArgumentException e){throw new IOException("Duplicate disc installer metadata.",e);}finally{MsiCloseHandle(db);}
    }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct CabinetFile {
        public IntPtr Name;public uint Size,Error;public ushort Date,Time,Attributes;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=260)]public string Target;
    }
    [StructLayout(LayoutKind.Sequential)]struct FilePaths {public IntPtr Target,Source;public uint Error,Flags;}
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]delegate uint CabinetCallback(IntPtr context,uint notification,IntPtr p1,IntPtr p2);
    [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool SetupIterateCabinetW(string cabinet,uint reserved,CabinetCallback callback,IntPtr context);
    public static void ExtractCabinet(string cab,string game,List<GameEntry> layout,CancellationToken cancel,Action<int,string> progress) {
        const uint FileInCabinet=0x11,NeedNewCabinet=0x12,FileExtracted=0x13;
        var expected=layout.ToDictionary(e=>e.Id,StringComparer.OrdinalIgnoreCase);var found=new HashSet<string>(StringComparer.OrdinalIgnoreCase);Exception failure=null;int done=0;
        // Validate every destination before handing any output path to native code.
        foreach(var e in layout){string target=InstallerCore.Under(game,e.Relative);if(target.Length>=260)throw new IOException("The installation path is too long for this disc format. Choose a shorter destination, such as C:\\Games\\HotD2VR.");Directory.CreateDirectory(Path.GetDirectoryName(target));}
        CabinetCallback callback=delegate(IntPtr context,uint notification,IntPtr p1,IntPtr p2) {
            try {
                if(failure!=null)return notification==FileInCabinet?0u:1223u;cancel.ThrowIfCancellationRequested();
                if(notification==FileInCabinet) {
                    var info=(CabinetFile)Marshal.PtrToStructure(p1,typeof(CabinetFile));string id=Marshal.PtrToStringUni(info.Name);GameEntry e;
                    if(!expected.TryGetValue(id,out e)||!found.Add(id)||info.Size!=e.Size)throw new IOException("Cabinet contents do not match the game manifest.");
                    info.Target=InstallerCore.Under(game,e.Relative);Marshal.StructureToPtr(info,p1,false);return 1;
                }
                if(notification==FileExtracted) {var info=(FilePaths)Marshal.PtrToStructure(p1,typeof(FilePaths));if(info.Error!=0)throw new IOException("Game file extraction failed (Windows error "+info.Error+").");File.SetAttributes(Marshal.PtrToStringUni(info.Target),FileAttributes.Normal);progress(18+(++done*10/layout.Count),"Extracting game ("+done+" / "+layout.Count+")...");}
                if(notification==NeedNewCabinet)throw new IOException("Multi-cabinet disc downloads are not supported.");return 0;
            }catch(Exception e){failure=e;return notification==FileInCabinet?0u:1223u;}
        };
        bool ok=SetupIterateCabinetW(cab,0,callback,IntPtr.Zero);int error=Marshal.GetLastWin32Error();GC.KeepAlive(callback);
        if(failure!=null)throw failure;cancel.ThrowIfCancellationRequested();if(!ok)throw new IOException("Cannot extract the game cabinet (Windows error "+error+").");
        if(found.Count!=layout.Count||done!=layout.Count)throw new IOException("The cabinet is missing game files.");
        foreach(var e in layout)if(new FileInfo(InstallerCore.Under(game,e.Relative)).Length!=e.Size)throw new IOException("Extracted game file size mismatch.");
    }
}
}
