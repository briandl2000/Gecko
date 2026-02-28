$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
& python (Join-Path $ScriptDir "..\scripts\cli.py") @args
exit $LASTEXITCODE
