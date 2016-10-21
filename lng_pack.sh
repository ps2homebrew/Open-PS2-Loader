#!/bin/bash
# Lang Packer for Open-PS2-Loader by Caio99BR <caiooliveirafarias0@gmail.com>

# Set variables
_dir=$(pwd)
_bdir="/tmp/opl_lng"
_rev=$(cat ${_dir}/DETAILED_CHANGELOG | grep "commit" | head -1 | cut -d " " -f 1 | cut -c 7-)

# Print a list
printf "$(ls ${_dir}/lng/ | cut -c 6- | rev | cut -c 5- | rev)" > /tmp/opl_lng_list

# Copy like Jay-Jay format
while IFS= read -r file
do
	mkdir -p ${_bdir}/${file}${_rev}/
	cp ${_dir}/lng/lang_${file}.lng ${_bdir}/${file}${_rev}/lang_${file}${_rev}.lng
	if [ -e thirdparty/font_${file}.ttf ]
	then
		cp ${_dir}/thirdparty/font_${file}.ttf ${_bdir}/${file}${_rev}/font_${file}.ttf
	fi
done < /tmp/opl_lng_list

# Add CREDITS for Translators :)
if [ -e ${_dir}/CREDITS_LNG ]
then
	cp ${_dir}/CREDITS_LNG ${_bdir}/CREDITS_LNG
fi

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
zip -r OPNPS2LD_LANGS-r${_rev}.zip *
cp ${_bdir}/OPNPS2LD_LANGS-r${_rev}.zip ${_dir}/OPNPS2LD_LANGS-r${_rev}.zip

# Cleanup
cd ${_dir}
rm -rf ${_bdir}/ /tmp/opl_lng_list
unset _dir _bdir _rev
