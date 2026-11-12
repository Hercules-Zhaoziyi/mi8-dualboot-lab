param([string]$Adb='adb',[string]$Serial='')
$ErrorActionPreference='Stop'
$argsForDevice=@();if($Serial){$argsForDevice=@('-s',$Serial)}
$values=& $Adb @argsForDevice shell 'getprop ro.product.device; getprop ro.build.version.incremental; getprop sys.boot_completed'
if($LASTEXITCODE -ne 0){throw 'ADB query failed; no device change was attempted'}
@{product=$values[0];android_build=$values[1];boot_completed=$values[2];serial_included=$false;read_only=$true} | ConvertTo-Json
