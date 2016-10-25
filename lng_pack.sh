#!/bin/bash
# Lang Packer for Open-PS2-Loader by Caio99BR <caiooliveirafarias0@gmail.com>

# Set variables
_dir=$(pwd)
_bdir="/tmp/opl_lng"
opl_revision=$(($(cat ${_dir}/DETAILED_CHANGELOG | grep "rev" | head -1 | cut -d " " -f 1 | cut -c 4-) + 1))
opl_git=$(git -C ${_dir}/ rev-parse --short=7 HEAD 2>/dev/null)
if [ ${opl_git} ]; then export opl_git=-${opl_git}; fi

# Print a list
printf "$(ls ${_dir}/lng/ | cut -c 6- | rev | cut -c 5- | rev)" > /tmp/opl_lng_list

# Copy format
while IFS= read -r file
do
	mkdir -p ${_bdir}/${file}-${opl_revision}/
	cp ${_dir}/lng/lang_${file}.lng ${_bdir}/${file}-${opl_revision}/lang_${file}.lng
	if [ -e thirdparty/font_${file}.ttf ]
	then
		cp ${_dir}/thirdparty/font_${file}.ttf ${_bdir}/${file}-${opl_revision}/font_${file}.ttf
	fi
done < /tmp/opl_lng_list

(cat << EOF) > ${_bdir}/README
-----------------------------------------------------------------------------

  Copyright 2009-2010, Ifcaro & jimmikaelkael
  Licenced under Academic Free License version 3.0
  Review Open PS2 Loader README & LICENSE files for further details.

-----------------------------------------------------------------------------

Open PS2 Loader Official Translations (25 August 2016)

HOW TO INSTALL:
1. make sure you are running latest OPL
2. transfer both the the LANGUAGE FILE (.lng) and the FONT FILE (.ttf) into your OPL directory of your memory card
 a. IMPORTANT: Do not rename the files
 b. NOTE: Some languages don't require a FONT file, so it won't be included
3. run OPL
4. go to OPL Settings
5. open the Display settings page
6. select your new language file
7. press Ok at the bottom of the page
8. then save your settings so next time you run OPL, it will load it with your preferred language file
9. done!
EOF

# Lets pack it!
cd ${_bdir}/
zip -r ${_dir}/OPNPS2LD_LANGS-${opl_revision}${opl_git}.zip *

# Cleanup
cd ${_dir}
rm -rf ${_bdir}/ /tmp/opl_lng_list
unset _dir _bdir opl_revision
