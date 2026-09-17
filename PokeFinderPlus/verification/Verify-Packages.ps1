param(
 [string]$OutputDirectory='C:\PokeFinderDev\PokeFinderPlus-package',
 [string]$EvidenceDirectory='C:\PokeFinderDev\build-plus-parent',
 [string]$InstalledCleanDirectory
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public class PackageVerification { [DllImport("kernel32.dll")] public static extern bool GetExitCodeProcess(IntPtr handle,out uint code); [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y); [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra); }'
$env:PATH="$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_PLUGIN_PATH=$null
$env:QT_QPA_PLATFORM_PLUGIN_PATH=$null
$env:QT_LOGGING_TO_CONSOLE=$null
$env:QT_DEBUG_PLUGINS='1'
$env:QT_FORCE_STDERR_LOGGING='1'
$root=[System.Windows.Automation.AutomationElement]::RootElement
function Nodes {
 $condition=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$script:app.Id)
 foreach($window in $root.FindAll([System.Windows.Automation.TreeScope]::Children,$condition)){
  foreach($node in $window.FindAll([System.Windows.Automation.TreeScope]::Subtree,[System.Windows.Automation.Condition]::TrueCondition)){$node}
 }
}
function MenuItem([string]$name){
 $node=Nodes | Where-Object {$_.Current.ControlType -eq [System.Windows.Automation.ControlType]::MenuItem -and $_.Current.Name -eq $name} | Select-Object -First 1
 if(!$node){throw "Missing menu entry $name"}; $node
}
function CheckExit($process,$handle){
 if(!$process.WaitForExit(5000)){throw "PID $($process.Id) did not exit"}
 [uint32]$code=0; [void][PackageVerification]::GetExitCodeProcess($handle,[ref]$code)
 "PID=$($process.Id) exit=0x{0:X8}" -f $code
 if($code -ne 0){throw 'Abnormal exit'}
}
$cases=@(
 @{Name='clean';Directory=(Join-Path $OutputDirectory 'release\PokeFinder+');Test=$false},
 @{Name='test';Directory=(Join-Path $OutputDirectory 'test\PokeFinder+');Test=$true}
)
if($InstalledCleanDirectory){$cases+=@{Name='installed-clean';Directory=$InstalledCleanDirectory;Test=$false}}
foreach($case in $cases){
 $directory=$case.Directory
 $launcher=Start-Process -FilePath (Join-Path $directory 'PokeFinder+.exe') -WorkingDirectory $env:TEMP -RedirectStandardOutput (Join-Path $EvidenceDirectory ($case.Name+'.stdout.log')) -RedirectStandardError (Join-Path $EvidenceDirectory ($case.Name+'.stderr.log')) -PassThru
 $launcherHandle=$launcher.Handle
 Start-Sleep -Seconds 4
 $child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($launcher.Id)" | Where-Object Name -eq 'PokeFinderPlusApp.exe'
 if(!$child){throw "No running app for $($case.Name)"}
 $script:app=Get-Process -Id $child.ProcessId
 $appHandle=$app.Handle
 $main=Nodes | Where-Object {$_.Current.ClassName -eq 'MainWindow'} | Select-Object -First 1
 if(!$main -or $main.Current.IsOffscreen -or !$main.Current.IsEnabled){throw "No usable main window for $($case.Name)"}
 "$($case.Name): visible MainWindow=$($main.Current.Name)"
 $expectedParent=[IO.Path]::GetDirectoryName($directory)
 foreach($module in ($app.Modules | Where-Object {$_.ModuleName -match '^Qt6|^qwindows'})){
  "loaded=$($module.FileName)"
  if(!$module.FileName.StartsWith($expectedParent+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Loaded Qt outside the parent release'}
 }
 $main.SetFocus()
 (MenuItem 'Plugins').GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
 Start-Sleep -Milliseconds 300
 $popup=Nodes | Where-Object {$_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Menu -and !$_.Current.IsOffscreen} | Select-Object -First 1
 if(!$popup){throw 'Plugins popup missing'}
 $names=@($popup.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition) | ForEach-Object {$_.Current.Name})
 $expected=if($case.Test){'Annotations|Plugin Manager...'}else{'Plugin Manager...'}
 "menu=$($names -join '|')"
 if(($names -join '|') -ne $expected){throw 'Unexpected Plugins menu'}
 $rect=$popup.Current.BoundingRectangle
 $bitmap=[System.Drawing.Bitmap]::new([int]$rect.Width,[int]$rect.Height)
 $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
 try{$graphics.CopyFromScreen([int]$rect.X,[int]$rect.Y,0,0,$bitmap.Size); $bitmap.Save((Join-Path $EvidenceDirectory ($case.Name+'-menu.png')))}finally{$graphics.Dispose();$bitmap.Dispose()}
 if($case.Test){
  $action=MenuItem 'Annotations'
  foreach($expectedState in @('Off','On')){
   $bounds=$action.Current.BoundingRectangle
   [void][PackageVerification]::SetCursorPos([int]($bounds.X+$bounds.Width/2),[int]($bounds.Y+$bounds.Height/2))
   [PackageVerification]::mouse_event(2,0,0,0,[UIntPtr]::Zero); [PackageVerification]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
   Start-Sleep -Milliseconds 250
   $expectedValue=if($expectedState -eq 'Off'){'false'}else{'true'}
   $ini=Get-Content -LiteralPath (Join-Path $directory 'plugins.ini') -Raw
   $action=MenuItem 'Annotations'
   if($ini -notmatch "enabled=$expectedValue" -or $action.Current.IsOffscreen){throw 'Plugin toggle/persistence/menu retention failed'}
   $state=$expectedState
   "Annotations=$state menu-open=True"
  }
 }
 $bounds=(MenuItem 'Plugin Manager...').Current.BoundingRectangle
 [void][PackageVerification]::SetCursorPos([int]($bounds.X+$bounds.Width/2),[int]($bounds.Y+$bounds.Height/2))
 Start-Sleep -Milliseconds 150
 [PackageVerification]::mouse_event(2,0,0,0,[UIntPtr]::Zero); [PackageVerification]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
 $dialog=$null
 for($attempt=0;$attempt -lt 20 -and !$dialog;$attempt++){
  Start-Sleep -Milliseconds 150
  $dialog=Nodes | Where-Object {$_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and $_.Current.Name -eq 'Plugin Manager'} | Select-Object -First 1
 }
 if(!$dialog){throw 'Plugin Manager did not open'}
 $dialog.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close()
 Start-Sleep -Seconds 3
 if($app.HasExited -or !$main.Current.IsEnabled){throw 'Main window did not remain usable'}
 $main.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close()
 CheckExit $app $appHandle
 CheckExit $launcher $launcherHandle
 if(!$case.Test){
  if(@(Get-ChildItem -LiteralPath (Join-Path $directory 'plugins') -Force).Count -ne 0){throw 'Clean plugins folder not empty'}
  if(@(Get-ChildItem -LiteralPath (Join-Path $directory 'data') -Force).Count -ne 0){throw 'Clean data folder not empty'}
  if((Get-Item -LiteralPath (Join-Path $directory 'plugins.ini')).Length -ne 0){throw 'Clean default config was modified'}
  if(Test-Path -LiteralPath (Join-Path $directory 'PokeFinderPlus.log')){throw 'Clean app wrote a diagnostic log'}
  "$($case.Name): zero plugin DLLs, empty data/default ini, no diagnostic log"
 }else{
  $log=Get-Content -LiteralPath (Join-Path $directory 'PokeFinderPlus.log') -Raw
  if($log -notmatch 'load .*Annotations.dll success=yes' -or $log -notmatch 'initialize success=yes' -or $log -notmatch 'shutdown Annotations'){throw 'Missing successful plugin lifecycle diagnostics'}
  'Test plugin discovered, initialized, toggled and shut down cleanly'
 }
}
