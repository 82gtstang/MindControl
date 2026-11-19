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
  $cudaRoots = @()
  if ($env:CUDA_PATH) { $cudaRoots += $env:CUDA_PATH }
  $cudaRoots += @(Get-ChildItem 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -ExpandProperty FullName)
  $cudaRoot = $cudaRoots | Where-Object { Test-Path (Join-Path $_ 'bin\nvcc.exe') } | Select-Object -First 1
  if (!$cudaRoot) {
    throw 'CUDA Toolkit nvcc.exe not found. PyTorch CUDA wheels do not include the CUDA compiler. Install NVIDIA CUDA Toolkit 12.1 with Visual Studio C++ integration on this runner, then rerun.'
  }
  $env:CUDA_PATH = $cudaRoot
  $env:CUDA_HOME = $cudaRoot
  $env:CUDAToolkit_ROOT = $cudaRoot
  $env:PATH = (Join-Path $cudaRoot 'bin') + ';' + $env:PATH
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
