setlocal
set VSCOMNTOOLS=%VS100COMNTOOLS%;%VS110COMNTOOLS%;%VS120COMNTOOLS%
for %%i in (vsvars32.bat) do call "%%~$VSCOMNTOOLS:i"
msxsl character-sets.xml character-sets.c.xsl > character-sets.c
msxsl character-sets.xml character-sets.rc.xsl > character-sets.rc
msxsl character-sets.xml character-sets.def.xsl > character-sets.def
rc character-sets.rc
cl character-sets.c character-sets-ie5.c /link /dll /noentry /map character-sets.res /def:character-sets.def /machine:x86 /out:character-sets.dll
