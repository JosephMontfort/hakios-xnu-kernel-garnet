# xl.ps1 - PowerShell port of xl: drive the XNU loader on the phone over USB (fastboot).
#
#   xl.ps1 enter            get the phone into the resident loader (from Android or stock fastboot)
#   xl.ps1 run [N]          upload the current kernel, run it (optionally stop at checkpoint N), fetch the log
#   xl.ps1 ramdisk [FILE]   upload the big system image (default userland/system.apfs) once; it stays in
#                           the phone's RAM across kernel runs and is the root filesystem (/dev/md0)
#                           ("xl.ps1 ramdisk none" forgets it; the image sent with the kernel is then the root)
#   xl.ps1 screenshot [F]   save what is in the phone's framebuffer as a PNG (default logs/screen.png);
#                           works during a run too (from another terminal)
#   xl.ps1 log              fetch the loader's current log; during a run, the kernel's log so far as well
#   xl.ps1 stop             end the run in progress now (the kernel returns as when its time is up)
#   xl.ps1 hang             watchdog test: make the loader hang on purpose
#   xl.ps1 reboot           cold-reset the phone (back to Android)
#   xl.ps1 reload           restart the phone into a freshly built loader (after changing xnuloader/)
#   xl.ps1 memmap           the phone's memory map as its firmware reports it
#   xl.ps1 state            where is the phone right now
#
# Logs land in logs/. Environment for `run` (same names as xl):
#   XL_CMDLINE  extra kernel boot-args; default "-v"
#   XL_SECS     seconds after which the kernel returns to the loader on its own (default 8)
#   XL_SB_DELAY seconds SpringBoard waits before it starts (default 8)
#   XL_KERNEL   the kernel to run (default prebuilt/mach.development.bcm2837)
#   XL_RAMDISK  the image sent with the kernel (default userland/root.apfs; "none" sends none)
#
# Not ported (they need python tools/*.py): recording, sound, camera. Use the WSL xl for those.
# The garnet flow needs no Python: run builds its payload here, and enter returns at once
# once the loader is up (fastboot boot garnet-boot.img first, as usual).
param([string]$Command, [string]$Arg)

Set-Location $PSScriptRoot
if (-not $env:TEMP) { $env:TEMP = '/tmp' }
if (-not $env:PLATFORM_TOOLS) {
    $ptFile = Join-Path $PSScriptRoot 'path-plataform.txt'
    if (Test-Path $ptFile) {
        $saved = Get-Content $ptFile -TotalCount 1 -ErrorAction SilentlyContinue
        if ($saved) { $saved = $saved.Trim().TrimEnd(';').Trim('"').Trim() }
        if ($saved) { $env:PLATFORM_TOOLS = $saved }
    }
}
if ($env:PLATFORM_TOOLS) { $env:PLATFORM_TOOLS = $env:PLATFORM_TOOLS.Trim().TrimEnd(';').Trim('"').Trim() }
if ($env:PLATFORM_TOOLS -and (Test-Path $env:PLATFORM_TOOLS)) {
    $sep = if ($env:OS -eq 'Windows_NT') { ';' } else { ':' }
    if (($env:PATH -split [regex]::Escape($sep)) -notcontains $env:PLATFORM_TOOLS) {
        $env:PATH = "$env:PLATFORM_TOOLS$sep$env:PATH"
    }
}
$script:K = if ($env:XL_KERNEL) { $env:XL_KERNEL } else { 'prebuilt/mach.development.bcm2837' }
$script:L = if (Test-Path 'xnuloader/loader.efi') { 'xnuloader/loader.efi' } else { 'prebuilt/loader.efi' }
New-Item -ItemType Directory -Force -Path 'logs' | Out-Null
$script:PhoneBase = Join-Path $env:TEMP 'hackios-phone'
$script:PhoneLock = "$($script:PhoneBase).lock"
$script:PhoneRun = "$($script:PhoneBase).run"
$script:PhoneWho = "$($script:PhoneBase).who"
$script:PhoneRamdisk = "$($script:PhoneBase).ramdisk"
$script:LastExit = 0
$script:RunLockStream = $null

function Write-TextFile {
    param([string]$Path, [string]$Text)
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

# One fastboot at a time, like xl's flock: the lock file is held open exclusively
# for the call. Returns fastboot's output (stdout then stderr); $script:LastExit is its exit code.
function Invoke-Fastboot {
    param([string[]]$Arguments, [int]$TimeoutSec = 10, [switch]$NoLock)
    $stream = $null
    if (-not $NoLock) {
        $deadline = (Get-Date).AddSeconds(90)
        while ($true) {
            try { $stream = [System.IO.File]::Open($script:PhoneLock, 'OpenOrCreate', 'ReadWrite', 'None'); break }
            catch {
                if ((Get-Date) -gt $deadline) { $script:LastExit = 1; return '' }
                Start-Sleep -Milliseconds 100
            }
        }
    }
    $outFile = [System.IO.Path]::GetTempFileName()
    $errFile = [System.IO.Path]::GetTempFileName()
    try {
        $argString = ($Arguments | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
        try {
            $proc = Start-Process -FilePath 'fastboot' -ArgumentList $argString -NoNewWindow -PassThru `
                -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        } catch { $script:LastExit = 1; return '' }
        if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
            try { $proc.Kill() } catch {}
            $proc.WaitForExit()
            $script:LastExit = 1
        } else { $script:LastExit = $proc.ExitCode }
        $out = ''
        if (Test-Path $outFile) { $out = [System.IO.File]::ReadAllText($outFile) }
        if (Test-Path $errFile) {
            $err = [System.IO.File]::ReadAllText($errFile)
            if ($err) { $out += $err }
        }
        return $out
    } finally {
        if ($stream) { $stream.Close() }
        if (Test-Path $outFile) { Remove-Item $outFile -Force -ErrorAction SilentlyContinue }
        if (Test-Path $errFile) { Remove-Item $errFile -Force -ErrorAction SilentlyContinue }
    }
}

function Invoke-Adb {
    param([string[]]$Arguments)
    try {
        $p = Start-Process -FilePath 'adb' -ArgumentList ($Arguments -join ' ') -NoNewWindow -PassThru -Wait
        return $p.ExitCode
    } catch { return 1 }
}

function Get-AdbState {
    try {
        $o = & adb get-state 2>$null
        return ($o | Out-String).Trim()
    } catch { return '' }
}

function Test-LiveLoader {
    return (Invoke-Fastboot @('getvar', 'version')) -match 'version: 0\.([5-9]|[1-9][0-9])'
}

function Test-InFastboot {
    return (Invoke-Fastboot @('devices') -NoLock) -match 'fastboot'
}

function Test-IsLoader {
    return (Test-InFastboot) -and ((Invoke-Fastboot @('getvar', 'product')) -match 'product: xnuloader')
}

function Get-State {
    if (Test-IsLoader) { return 'loader' }
    if (Test-InFastboot) { return 'fastboot' }
    if ((Get-AdbState) -eq 'device') { return 'android' }
    return 'absent'
}

function Wait-ForState {
    param([string]$Wanted, [int]$Seconds)
    for ($i = 0; $i -lt $Seconds; $i += 2) {
        if ((Get-State) -eq $Wanted) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Get-ResultLine {
    param([string]$Text)
    return ($Text -split "`n" | Where-Object { $_ -match '^result:' } | Select-Object -First 1)
}

function Invoke-Enter {
    $st = Get-State
    switch ($st) {
        'loader' { return $true }
        'android' {
            $null = Invoke-Adb @('reboot', 'bootloader')
            if (-not (Wait-ForState 'fastboot' 60)) { Write-Host 'xl: phone did not reach fastboot'; return $false }
        }
        'absent' {
            Write-Host 'xl: waiting for the phone (Android or fastboot)...'
            for ($i = 0; $i -lt 180; $i += 3) {
                if ((Get-State) -ne 'absent') { break }
                Start-Sleep -Seconds 3
            }
            if ((Get-State) -eq 'absent') { Write-Host 'xl: phone not reachable'; return $false }
            return Invoke-Enter
        }
    }
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
        Write-Host 'xl: python is needed to build the loader image; boot it first: fastboot boot out/garnet-boot.img'
        return $false
    }
    & python xnuloader/mkpayload.py -o usb-payload.bin "loader.efi=$script:L" --usb >$null
    if ($LASTEXITCODE -ne 0) { Write-Host 'xl: image build failed'; return $false }
    & python ./mkxnuimg.py -p usb-payload.bin -o xnu-usb.img >$null
    if ($LASTEXITCODE -ne 0) { Write-Host 'xl: image build failed'; return $false }
    for ($try = 1; $try -le 2; $try++) {
        $out = Invoke-Fastboot @('boot', 'xnu-usb.img') -TimeoutSec 40
        if ($out -notmatch 'OKAY') { Start-Sleep -Seconds 3; continue }
        Start-Sleep -Seconds 6
        if (Wait-ForState 'loader' 30) { return $true }
        if ((Get-State) -ne 'fastboot') { break }
    }
    Write-Host "xl: loader did not come up on USB (state: $(Get-State))"
    return $false
}

function Fetch-Log {
    param([string]$File)
    if (Test-Path $File) { Remove-Item $File -Force }
    $null = Invoke-Fastboot @('get_staged', $File) -TimeoutSec 20
    if (-not (Test-Path $File)) { return $false }
    if ((Get-Item $File).Length -eq 0) { return $false }
    $bytes = [System.IO.File]::ReadAllBytes($File)
    $clean = $bytes | Where-Object { $_ -ne 0 }
    [System.IO.File]::WriteAllBytes($File, [byte[]]$clean)
    return $true
}

# mkpayload.py, reimplemented: header u32 "XNUP" + u32 body size; entries of
# name[16] + u32 size + u32 reserved + data padded to 16; end 24 NUL bytes.
function Build-Payload {
    param([string]$Out, [array]$Entries, [string]$Cmdline)
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $writeEntry = {
        param([string]$Name, [byte[]]$Data)
        $nameBytes = New-Object byte[] 16
        $ascii = [System.Text.Encoding]::ASCII.GetBytes($Name)
        [Array]::Copy($ascii, $nameBytes, [Math]::Min(16, $ascii.Length))
        $bw.Write($nameBytes)
        $bw.Write([uint32]$Data.Length)
        $bw.Write([uint32]0)
        $bw.Write($Data)
        $pad = (16 - ($Data.Length % 16)) % 16
        if ($pad -gt 0) { $bw.Write((New-Object byte[] $pad)) }
    }
    foreach ($e in $Entries) {
        & $writeEntry $e[0] ([System.IO.File]::ReadAllBytes($e[1]))
    }
    if ($Cmdline) {
        $cmdBytes = [System.Text.Encoding]::UTF8.GetBytes($Cmdline)
        $data = New-Object byte[] ($cmdBytes.Length + 1)
        [Array]::Copy($cmdBytes, $data, $cmdBytes.Length)
        & $writeEntry 'cmdline' $data
    }
    $bw.Write((New-Object byte[] 24))
    $bw.Flush()
    $body = $ms.ToArray()
    $bw.Close(); $ms.Close()
    $fs = [System.IO.File]::Create($Out)
    $bw2 = New-Object System.IO.BinaryWriter($fs)
    $bw2.Write([uint32]0x50554E58)
    $bw2.Write([uint32]$body.Length)
    $bw2.Write($body)
    $bw2.Close(); $fs.Close()
}

function Invoke-Run {
    param([string]$OemCmd, [string]$Label)
    if (-not (Invoke-Enter)) { return 1 }
    try {
        $script:RunLockStream = [System.IO.File]::Open($script:PhoneRun, 'OpenOrCreate', 'ReadWrite', 'None')
    } catch {
        $who = ''
        if (Test-Path $script:PhoneWho) { $who = Get-Content $script:PhoneWho -TotalCount 1 }
        Write-Host "xl: a run is in progress, started from $who (xl stop ends it)"
        return 1
    }
    Write-TextFile $script:PhoneWho ((Get-Location).Path)
    $stamp = Get-Date -Format 'HHmmss'
    $log = "logs/$stamp-$Label.log"
    if ($OemCmd -ne 'oem selftest') {
        $rd = if ($null -ne $env:XL_RAMDISK) { $env:XL_RAMDISK } else { 'userland/root.apfs' }
        if ($rd -eq 'none' -or $rd -eq 'resident') { $rd = '' }
        elseif (-not (Test-Path $rd)) { Write-Host "xl: ramdisk $rd not found"; return 1 }
        [Console]::Error.WriteLine("xl: running $($script:K)")
        if ((Test-Path $script:PhoneRamdisk) -and ((Get-Item $script:PhoneRamdisk).Length -gt 0)) {
            $parts = (Get-Content $script:PhoneRamdisk -TotalCount 1) -split "`t"
            if ($parts[0] -ne (Get-Location).Path) {
                $mb = [Math]::Floor([double]$parts[2] / 1048576)
                [Console]::Error.WriteLine("xl: the resident image was uploaded from $($parts[0]): $($parts[1]), $mb MB. Upload your own (./xl.ps1 ramdisk FILE) unless that is the one you want")
            }
        } else {
            [Console]::Error.WriteLine('xl: nothing here recorded who uploaded the resident image; if this run needs a particular one, upload it')
        }
        $args = if ($null -eq $env:XL_CMDLINE) { '-v' } else { $env:XL_CMDLINE }
        if (($args -like '* wifi=*') -or ($args -like '* wifi_step=*')) {
            if (Test-Path 'fw-wifi/mac') {
                $mac = (Get-Content 'fw-wifi/mac' -TotalCount 1) -replace '\s', ''
                $args = "$args wifi_mac=$mac"
            }
        }
        $secs = if ($env:XL_SECS) { [int]$env:XL_SECS } else { 8 }
        $sbDelay = if ($env:XL_SB_DELAY) { [int]$env:XL_SB_DELAY } else { 8 }
        $epoch = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        $cmdline = "loader_return_after=$secs epoch=$epoch sb_delay=$sbDelay $args"
        $entries = @(,@('kernel', $script:K))
        if ($rd) { $entries += ,@('ramdisk', $rd) }
        Build-Payload -Out 'run-payload.bin' -Entries $entries -Cmdline $cmdline
        $stageOut = Invoke-Fastboot @('stage', 'run-payload.bin') -TimeoutSec 30
        if ($stageOut -notmatch 'OKAY') { Write-Host 'xl: upload failed'; return 1 }
    }
    $live = Test-LiveLoader
    $null = Invoke-Fastboot ($OemCmd -split ' ')
    $result = ''
    if ($live -and $OemCmd -ne 'oem selftest') {
        $secs = if ($env:XL_SECS) { [int]$env:XL_SECS } else { 8 }
        $deadline = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() + $secs + 100
        $followed = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        $followedN = 0
        while ($true) {
            $rOut = Invoke-Fastboot @('getvar', 'result') -TimeoutSec 15
            $result = Get-ResultLine $rOut
            if ($result -and $result -notlike '*running*') { break }
            if ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() -ge $deadline) { $result = ''; break }
            if ($env:XL_FOLLOW -and (([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $followed) -ge [int]$env:XL_FOLLOW)) {
                $followed = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
                $logOut = Invoke-Fastboot @('oem', 'log')
                if ($logOut -match 'OKAY') {
                    $null = Fetch-Log "$log.live"
                    if ($env:XL_FOLLOW_KEEP) { $followedN++; Copy-Item "$log.live" "$log.live.$followedN" -Force -ErrorAction SilentlyContinue }
                }
            }
            Start-Sleep -Seconds 2
        }
    } else {
        $secs = if ($env:XL_SECS) { [int]$env:XL_SECS } else { 8 }
        $rOut = Invoke-Fastboot @('getvar', 'result') -TimeoutSec ($secs + 100)
        $result = Get-ResultLine $rOut
    }
    if (-not $result) {
        Write-Host 'xl: no answer - the kernel hung; waiting for the watchdog to reset the phone'
        if (Wait-ForState 'android' 120) { Write-Host 'xl: phone is back in Android' }
        else { Write-Host "xl: phone did not come back (state: $(Get-State))" }
        return 2
    }
    $null = Fetch-Log $log
    try {
        if (Test-Path 'logs/last.log') { Remove-Item 'logs/last.log' -Force }
        New-Item -ItemType SymbolicLink -Path 'logs/last.log' -Target (Split-Path $log -Leaf) | Out-Null
    } catch { Copy-Item $log 'logs/last.log' -Force -ErrorAction SilentlyContinue }
    Write-Host "$result  [$log]"
    if (($result -like '*hung*') -or ($result -like '*no-such-core*')) {
        Write-Host 'xl: the kernel did not come back from its core; restarting the phone'
        $null = Invoke-Fastboot @('reboot')
        return 2
    }
    return 0
}

function Invoke-Ramdisk {
    param([string]$File)
    if (-not (Invoke-Enter)) { return 1 }
    if ($File -eq 'none') {
        $null = Invoke-Fastboot @('oem', 'ramdisk', 'none')
        Remove-Item $script:PhoneRamdisk -Force -ErrorAction SilentlyContinue
        Write-Host 'xl: resident ramdisk dropped'
        return 0
    }
    $f = if ($File) { $File } else { 'userland/system.apfs' }
    if (-not (Test-Path $f)) { Write-Host "xl: $f not found"; return 1 }
    $t0 = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $size = (Get-Item $f).Length
    $out = Invoke-Fastboot @('oem', 'ramdisk')
    if ($out -notmatch 'OKAY') { Write-Host 'xl: the loader refused (no memory window?)'; return 1 }
    $out = Invoke-Fastboot @('stage', $f) -TimeoutSec 1800
    if ($out -notmatch 'OKAY') { Write-Host 'xl: upload failed'; return 1 }
    $dt = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $t0
    if ($dt -lt 1) { $dt = 1 }
    $mb = [Math]::Floor($size / 1048576)
    $rate = [Math]::Floor($mb / $dt)
    Write-Host "xl: $mb MB uploaded in $dt s ($rate MB/s)"
    Write-TextFile $script:PhoneRamdisk "$((Get-Location).Path)`t$f`t$size`n"
    return 0
}

function Invoke-Screenshot {
    param([string]$Out)
    if (-not (Invoke-Enter)) { return 1 }
    if (-not $Out) { $Out = 'logs/screen.png' }
    $geom = ''
    $gv = Invoke-Fastboot @('getvar', 'screen')
    foreach ($line in ($gv -split "`n")) {
        if ($line -match '^screen: ') { $geom = $line.Substring(8).Trim() }
    }
    $ok = $false
    if ($geom) {
        $so = Invoke-Fastboot @('oem', 'screenshot')
        if ($so -match 'OKAY') { $ok = $true }
    }
    if (-not $ok) { Write-Host 'xl: the loader has no display'; return 1 }
    $null = Invoke-Fastboot @('get_staged', 'logs/screen.raw') -TimeoutSec 60
    $parts = $geom -split '\s+'
    $width = [int]$parts[0]; $height = [int]$parts[1]; $stride = [int]$parts[2]
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppRgb)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
    $bits = $bmp.LockBits($rect, 'WriteOnly', $bmp.PixelFormat)
    $raw = [System.IO.File]::ReadAllBytes('logs/screen.raw')
    for ($y = 0; $y -lt $height; $y++) {
        $dst = [IntPtr]($bits.Scan0.ToInt64() + $y * $bits.Stride)
        [System.Runtime.InteropServices.Marshal]::Copy($raw, $y * $stride, $dst, $width * 4)
    }
    $bmp.UnlockBits($bits)
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    Remove-Item 'logs/screen.raw' -Force -ErrorAction SilentlyContinue
    Write-Host "xl: $Out ($geom)"
    return 0
}

function Test-NotAnothersRun {
    param([string]$Cmd)
    try {
        $t = [System.IO.File]::Open($script:PhoneRun, 'OpenOrCreate', 'ReadWrite', 'None')
        $t.Close()
        return $true
    } catch {}
    $who = ''
    if (Test-Path $script:PhoneWho) { $who = Get-Content $script:PhoneWho -TotalCount 1 }
    if (($who -eq (Get-Location).Path) -or $env:XL_FORCE) { return $true }
    [Console]::Error.WriteLine("xl: a run started from $who is in progress; XL_FORCE=1 ./xl.ps1 $Cmd ends it all the same")
    return $false
}

$allowed = @('', 'run', 'selftest', 'state', 'screenshot', 'log', 'stop', 'reboot')
if ($allowed -notcontains $Command) {
    try {
        $t = [System.IO.File]::Open($script:PhoneRun, 'OpenOrCreate', 'ReadWrite', 'None')
        $t.Close()
    } catch {
        Write-Host 'xl: a run is in progress: only state, screenshot, log, stop and reboot now'
        exit 1
    }
}

$code = 0
switch ($Command) {
    'enter' {
        if (Invoke-Enter) { Write-Host 'xl: loader is up' } else { $code = 1 }
    }
    'screenshot' { $code = Invoke-Screenshot $Arg }
    'ramdisk' { $code = Invoke-Ramdisk $Arg }
    'run' {
        if ($Arg) { $code = Invoke-Run "oem run $Arg" "run-cp$Arg" }
        else { $code = Invoke-Run 'oem run' 'run' }
    }
    'selftest' { $code = Invoke-Run 'oem selftest' 'selftest' }
    'log' {
        if (Invoke-Enter) {
            $null = Invoke-Fastboot @('oem', 'log')
            if (Fetch-Log 'logs/loader.log') { Get-Content 'logs/loader.log' }
        } else { $code = 1 }
    }
    'stop' {
        if (Test-NotAnothersRun 'stop') {
            $out = Invoke-Fastboot @('oem', 'stop')
            if ($out -match 'OKAY') { Write-Host 'xl: asked the kernel to come back' }
        } else { $code = 1 }
    }
    'memmap' {
        if (Invoke-Enter) {
            $null = Invoke-Fastboot @('oem', 'memmap')
            $null = Invoke-Fastboot @('oem', 'log')
            if (Fetch-Log 'logs/loader.log') {
                $lines = Get-Content 'logs/loader.log'
                $idx = -1
                for ($i = 0; $i -lt $lines.Count; $i++) {
                    if ($lines[$i] -match '^memory map') { $idx = $i; break }
                }
                if ($idx -ge 0) { $lines[$idx..($lines.Count - 1)] }
            }
        } else { $code = 1 }
    }
    'hang' {
        if (Invoke-Enter) {
            $null = Invoke-Fastboot @('oem', 'hang')
            Write-Host 'xl: waiting for the watchdog...'
            $t0 = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
            if (Wait-ForState 'android' 150) {
                $dt = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $t0
                Write-Host "xl: back in Android after $dt s"
            } else { Write-Host "xl: not back (state: $(Get-State))" }
        } else { $code = 1 }
    }
    'reboot' {
        if (Test-NotAnothersRun 'reboot') {
            $out = Invoke-Fastboot @('reboot')
            if ($out) { Write-Host $out }
        } else { $code = 1 }
    }
    'reload' {
        if ((Get-State) -eq 'loader') {
            $null = Invoke-Fastboot @('reboot')
            $null = Wait-ForState 'android' 120
        }
        if (Invoke-Enter) { Write-Host 'xl: loader is up' } else { $code = 1 }
    }
    'state' { Write-Output (Get-State) }
    default {
        Write-Host @'
xl.ps1 - drive the XNU loader on the phone over USB (fastboot protocol).
  xl.ps1 enter | run [N] | ramdisk [FILE] | screenshot [F] | log | stop
  xl.ps1 hang | reboot | reload | memmap | state | selftest
Logs land in logs/. Environment: XL_CMDLINE (default "-v"), XL_SECS (default 8),
XL_SB_DELAY (default 8), XL_KERNEL, XL_RAMDISK (default userland/root.apfs).
Recording, sound and camera are not ported (they need python tools/*.py); use the WSL xl for those.
'@
    }
}
if ($script:RunLockStream) { $script:RunLockStream.Close() }
exit $code
