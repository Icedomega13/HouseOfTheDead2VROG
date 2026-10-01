function Set-Hotd2GraphicsQuality {
    param([Parameter(Mandatory=$true)][string]$ConfigPath,
        [ValidateSet(-1,0,2,4,8,16)][int]$Antialiasing=-1,
        [ValidateRange(0,16)][int]$AnisotropicFiltering=0)
    $text=Get-Content -LiteralPath $ConfigPath -Raw
    $section=[regex]::Match($text,'(?ms)^\[DirectX\]\s*\r?\n.*?(?=^\[|\z)')
    if(!$section.Success) {throw 'Graphics backend configuration lacks [DirectX].'}
    $aa=if($Antialiasing -lt 0){'appdriven'}elseif($Antialiasing -eq 0){'off'}else{"${Antialiasing}x"}
    $filter=if($AnisotropicFiltering -eq 0){'appdriven'}else{"$AnisotropicFiltering"}
    $keep=if($AnisotropicFiltering -gt 0){'true'}else{'false'}
    $updated=$section.Value
    foreach($setting in @(@('Antialiasing',$aa),@('Filtering',$filter),@('KeepFilterIfPointSampled',$keep))) {
        $pattern='(?m)^'+$setting[0]+'[ \t]*=.*$'
        if([regex]::Matches($updated,$pattern).Count -ne 1) {throw "Missing or duplicate graphics setting: $($setting[0])"}
        $updated=[regex]::Replace($updated,$pattern,($setting[0]+' = '+$setting[1]))
    }
    ($text.Substring(0,$section.Index)+$updated+$text.Substring($section.Index+$section.Length)) |
        Set-Content -LiteralPath $ConfigPath -Encoding ascii
}
