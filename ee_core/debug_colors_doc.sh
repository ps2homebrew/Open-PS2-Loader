#!/bin/bash
# automated listing of debug colors. with small description and location in source code
echo "# EECORE Color automated documentation"
echo "## Static Debug Colors:"
echo "source file | line | Color (BGR) | Category | Description"
echo ":---------- | :--- | :---------: | :------: | :--------- "
grep -Eon "DBGCOL\(.*\);" src/* | sed -E 's/(.*):([0-9]*):DBGCOL\((.*), (.*), "(.*)"\).*/\1 | \2 | \3 | \4 | \5/g' | sed -E "s/0x([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})/\!\[0x\1\2\3\](https:\/\/img.shields.io\/badge\/\1\2\3-\3\2\1\?style=for-the-badge)/g"

echo "## Blinking Debug Colors:"
#DBGCOL_BLNK(blinkCount, colour, forever, type, description)
echo "source file | line | Blinks | Color (BGR) | Blinks forever? | Category | Description"
echo ":---------- | :--- | :----- | :---------: | :-------------: | :------: | :--------- "
grep -Eon "DBGCOL_BLNK\(.*\);" src/* | sed -E 's/(.*):([0-9]*):DBGCOL_BLNK\((.*), (.*), (.*), (.*), "(.*)"\).*/\1 | \2 | \3 | \4 | **\5** | \6 | \7/g' | sed -E "s/0x([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})/\!\[0x\1\2\3\](https:\/\/img.shields.io\/badge\/\1\2\3-\3\2\1\?style=for-the-badge)/g"

