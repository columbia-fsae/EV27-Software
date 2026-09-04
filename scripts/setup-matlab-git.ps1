<#
.SYNOPSIS
    One-time per-machine setup for Simulink/MATLAB git integration.

.DESCRIPTION
    .gitattributes marks *.slx/*.slxc as `merge=mlAutoMerge` so git routes
    conflicting model changes through MATLAB's own 3-way merge instead of
    failing (or worse, silently corrupting a binary model file). That merge
    driver has to point at an absolute path to your local MATLAB install, so
    it can't be committed to the repo — every contributor runs this once.

    Run from PowerShell:
        .\scripts\setup-matlab-git.ps1

.NOTES
    Wires up three *global* (not per-repo) git settings:
      - merge.mlAutoMerge.driver  (used automatically on *.slx conflicts)
      - mergetool.mlMerge.cmd     (used by `git mergetool`)
      - difftool.mlDiff.cmd       (used by `git difftool`)
#>

$installs = Get-ChildItem "C:\Program Files\MATLAB" -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName "bin\win64\mlAutoMerge.bat") } |
    Sort-Object Name -Descending

if (-not $installs) {
    Write-Error "No MATLAB install with Simulink's git merge tools found under C:\Program Files\MATLAB.`nOpen MATLAB, open a Simulink Project, and enable 'Support 3-way merging of models' under Project > Source Control settings, then re-run this script."
    exit 1
}

$matlabBin = Join-Path $installs[0].FullName "bin\win64"
Write-Host "Using MATLAB at: $matlabBin"

git config --global merge.mlAutoMerge.name "MATLAB/Simulink automatic merge"
git config --global merge.mlAutoMerge.driver "`"$matlabBin\mlAutoMerge.bat`" %O %A %B %A"
git config --global mergetool.mlMerge.cmd "`"$matlabBin\mlMerge.bat`" `$BASE `$LOCAL `$REMOTE `$MERGED"
git config --global difftool.mlDiff.cmd "`"$matlabBin\mlDiff.bat`" `$LOCAL `$REMOTE"

Write-Host "Done. *.slx/*.slxc conflicts will now route through MATLAB's merge tool."
