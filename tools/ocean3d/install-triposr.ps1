param([string]$Root = "$env:LOCALAPPDATA\Ocean3D")
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Root | Out-Null
$python = Get-Command python -ErrorAction SilentlyContinue
if (!$python) { throw 'Python not found' }
& $python.Source -m venv (Join-Path $Root 'venv')
if ($LASTEXITCODE -ne 0) { throw 'venv creation failed' }
$py = Join-Path $Root 'venv\Scripts\python.exe'
& $py -m pip install --upgrade pip setuptools wheel
if ($LASTEXITCODE -ne 0) { throw 'pip bootstrap failed' }
$archive = Join-Path $Root 'triposr.zip'
Invoke-WebRequest -Uri 'https://github.com/VAST-AI-Research/TripoSR/archive/refs/heads/main.zip' -OutFile $archive -UseBasicParsing
Expand-Archive -Path $archive -DestinationPath $Root -Force
$source = Join-Path $Root 'TripoSR-main'
if (!(Test-Path (Join-Path $source 'run.py'))) { throw 'TripoSR source not downloaded' }
& $py -m pip install 'torch==2.3.1' 'torchvision==0.18.1' --index-url https://download.pytorch.org/whl/cu121
if ($LASTEXITCODE -ne 0) { throw 'PyTorch install failed' }
Push-Location $source
try {
  $requirements = Join-Path $Root 'requirements-no-git.txt'
  $lines = Get-Content 'requirements.txt' | ForEach-Object {
    $line = $_
    if ($line.StartsWith('git+https://github.com/')) {
      $repoPath = $line.Substring('git+https://github.com/'.Length).Split('#')[0]
      if ($repoPath.EndsWith('.git')) { $repoPath = $repoPath.Substring(0, $repoPath.Length - 4) }
      'https://github.com/' + $repoPath + '/archive/refs/heads/main.zip'
    } else { $line }
  }
  $lines | Set-Content -Path $requirements -Encoding UTF8
  & $py -m pip install scikit-build-core cmake ninja pybind11
  if ($LASTEXITCODE -ne 0) { throw 'Native build tools installation failed' }
  $pybindCmake = (& $py -m pybind11 --cmakedir | Select-Object -Last 1).Trim()
  if (!(Test-Path $pybindCmake)) { throw 'pybind11 CMake directory not found' }
  $env:pybind11_DIR = $pybindCmake
  $cudaRoots = @($env:CUDA_PATH_V12_1, $env:CUDA_PATH, 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.1')
  $cudaRoot = $cudaRoots | Where-Object {
    $_ -and (Test-Path (Join-Path $_ 'bin\nvcc.exe')) -and
      ((& (Join-Path $_ 'bin\nvcc.exe') --version | Out-String) -match 'release 12\.1,')
  } | Select-Object -First 1
  if (!$cudaRoot) {
    throw 'CUDA Toolkit nvcc.exe not found. PyTorch CUDA wheels do not include the CUDA compiler. Install NVIDIA CUDA Toolkit 12.1 on this runner, then rerun.'
  }
  $env:CUDA_PATH = $cudaRoot
  $env:CUDA_HOME = $cudaRoot
  $env:CUDAToolkit_ROOT = $cudaRoot
  $env:PATH = (Join-Path $cudaRoot 'bin') + ';' + $env:PATH
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (!(Test-Path $vswhere)) { throw 'Visual Studio Installer vswhere.exe not found' }
  $installations = @(& $vswhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
  $toolsets = foreach ($installation in $installations) {
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
    if (!(Test-Path $vcvars)) { continue }
    Get-ChildItem (Join-Path $installation 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -match '^14\.3\d\.\d+$' -and (Test-Path (Join-Path $_.FullName 'bin\Hostx64\x64\cl.exe')) } |
      ForEach-Object { [pscustomobject]@{ Version = [version]$_.Name; Setup = $vcvars; Directory = $_.FullName } }
  }
  $toolset = $toolsets | Sort-Object Version -Descending | Select-Object -First 1
  if (!$toolset) {
    throw 'CUDA 12.1 needs an MSVC 19.3x compiler. Install the Visual Studio 2022 v143 x64/x86 toolset v14.38 or v14.39 and a Windows SDK, then rerun.'
  }
  $vcSetup = 'call "{0}" x64 -vcvars_ver={1} >nul && set' -f $toolset.Setup, $toolset.Version
  $compilerEnv = & $env:ComSpec /d /c $vcSetup
  if ($LASTEXITCODE -ne 0) { throw "Failed to initialize MSVC toolset $($toolset.Version)" }
  foreach ($line in $compilerEnv) {
    if ($line -match '^([^=]+)=(.*)$') {
      [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
  }
  $runtimeLib = Join-Path $toolset.Directory 'lib\x64'
  if (!(Test-Path (Join-Path $runtimeLib 'MSVCRT.lib') -PathType Leaf)) {
    throw "MSVC $($toolset.Version) is missing its x64 release runtime import library: $runtimeLib\MSVCRT.lib. Repair the v143 14.39 x64/x86 build tools in Visual Studio Installer."
  }
  $env:LIB = $runtimeLib + ';' + $env:LIB
  Write-Host "Verified MSVC x64 runtime import library in $runtimeLib"
  $cl = Join-Path $toolset.Directory 'bin\Hostx64\x64\cl.exe'
  $nvcc = Join-Path $cudaRoot 'bin\nvcc.exe'
  $cmakeNvcc = $nvcc.Replace('\', '/')
  $cmakeCl = $cl.Replace('\', '/')
  $env:CMAKE_GENERATOR = 'Ninja'
  $env:CMAKE_ARGS = "-DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_CUDA_COMPILER=""$cmakeNvcc"" -DCMAKE_CUDA_HOST_COMPILER=""$cmakeCl"""
  $env:PATH = (Join-Path $Root 'venv\Scripts') + ';' + $env:PATH
  Write-Host "CUDA host compiler: $cl"
  Write-Host "CUDA Toolkit: $cudaRoot"
  & (Join-Path $cudaRoot 'bin\nvcc.exe') --version
  & $py -m pip install --no-build-isolation -r $requirements
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR requirements failed' }
  & $py -c "import torch; print('CUDA_AVAILABLE',torch.cuda.is_available()); print('TORCH_VERSION',torch.__version__); assert torch.cuda.is_available(), 'CUDA not available'"
  if ($LASTEXITCODE -ne 0) { throw 'CUDA smoke test failed' }
  & $py run.py --help
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR CLI smoke test failed' }
  Write-Host 'OCEAN3D_TRIPOSR_INSTALLED'
} finally { Pop-Location }
