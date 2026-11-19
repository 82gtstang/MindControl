param([string]$Root = "$env:LOCALAPPDATA\Ocean3D")
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Root | Out-Null
$python = (Get-Command python -ErrorAction SilentlyContinue)
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
  # pip VCS dependencies require git.exe, which is absent on this Windows runner.
  # Install GitHub-hosted Python dependencies from source archives instead.
  $requirements = Join-Path $Root 'requirements-no-git.txt'
  $lines = Get-Content 'requirements.txt' | ForEach-Object {
    if ($_ -match '^git\+https://github\.com/([^/]+)/([^/#]+?)(?:\.git)?(?:#.*)?
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR requirements failed' }
  & $py -c "import torch; print('CUDA_AVAILABLE',torch.cuda.is_available()); print('TORCH_VERSION',torch.__version__); assert torch.cuda.is_available(), 'CUDA not available'"
  if ($LASTEXITCODE -ne 0) { throw 'CUDA smoke test failed' }
  & $py run.py --help
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR CLI smoke test failed' }
  Write-Host 'OCEAN3D_TRIPOSR_INSTALLED'
} finally { Pop-Location }
) {
      'https://github.com/' + $Matches[1] + '/' + $Matches[2] + '/archive/refs/heads/main.zip'
    } else { $_ }
  }
  $lines | Set-Content -Path $requirements -Encoding UTF8
  & $py -m pip install -r $requirements
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR requirements failed' }
  & $py -c "import torch; print('CUDA_AVAILABLE',torch.cuda.is_available()); print('TORCH_VERSION',torch.__version__); assert torch.cuda.is_available(), 'CUDA not available'"
  if ($LASTEXITCODE -ne 0) { throw 'CUDA smoke test failed' }
  & $py run.py --help
  if ($LASTEXITCODE -ne 0) { throw 'TripoSR CLI smoke test failed' }
  Write-Host 'OCEAN3D_TRIPOSR_INSTALLED'
} finally { Pop-Location }
