#!/bin/bash
# Lang Packer for Open-PS2-Loader
# Made by Caio99BR <caiooliveirafarias0@gmail.com>
# Reworked by Doctor Q <Dr-Q@users.noreply.github.com>

# Set variables
CURRENT_DIR=$(pwd)
BUILD_DIR="/tmp/OPL_LANG"
LANG_LIST="/tmp/OPL_LANG_LIST"
make oplversion 2>/dev/null
if [ $? == "0" ]
then
	export OPL_VERSION=$(make oplversion)
else
	echo "Falling back to old OPL Lang Pack"
	VERSION=$(cat "${CURRENT_DIR}/Makefile" | grep "VERSION =" | head -1 | cut -d " " -f 3)
	SUBVERSION=$(cat "${CURRENT_DIR}/Makefile" | grep "SUBVERSION =" | head -1 | cut -d " " -f 3)
	PATCHLEVEL=$(cat "${CURRENT_DIR}/Makefile" | grep "PATCHLEVEL =" | head -1 | cut -d " " -f 3)
	REVISION=$(($(cat "${CURRENT_DIR}/DETAILED_CHANGELOG" | grep "rev" | head -1 | cut -d " " -f 1 | cut -c 4-) + 1))
	EXTRAVERSION=$(cat "${CURRENT_DIR}/Makefile" | grep "EXTRAVERSION =" | head -1 | cut -d " " -f 3)
	if [ "${EXTRAVERSION}" != "" ]; then EXTRAVERSION=-${EXTRAVERSION}; fi
	GIT_HASH=$(git -C "${CURRENT_DIR}/" rev-parse --short=7 HEAD 2>/dev/null)
	if [ "${GIT_HASH}" != "" ]; then GIT_HASH=-${GIT_HASH}; fi
	export OPL_VERSION=${VERSION}.${SUBVERSION}.${PATCHLEVEL}.${REVISION}${EXTRAVERSION}${GIT_HASH}
fi

# Print a list
printf "$(ls ${CURRENT_DIR}/lng/ | cut -c 6- | rev | cut -c 5- | rev)" > "${LANG_LIST}"

# Copy format
while IFS= read -r CURRENT_FILE
do
	mkdir -p "${BUILD_DIR}/${CURRENT_FILE}-${OPL_VERSION}/"
	cp "${CURRENT_DIR}/lng/lang_${CURRENT_FILE}.lng" "${BUILD_DIR}/${CURRENT_FILE}-${OPL_VERSION}/lang_${CURRENT_FILE}.lng"
	if [ -e "thirdparty/font_${CURRENT_FILE}.ttf" ]
	then
		cp "${CURRENT_DIR}/thirdparty/font_${CURRENT_FILE}.ttf" "${BUILD_DIR}/${CURRENT_FILE}-${OPL_VERSION}/font_${CURRENT_FILE}.ttf"
	fi
done < ${LANG_LIST}

(cat << EOF) > ${BUILD_DIR}/README
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
cd ${BUILD_DIR}/
zip -r "${CURRENT_DIR}/OPNPS2LD_LANGS-${OPL_VERSION}.zip" ./*
if [ -f "${CURRENT_DIR}/OPNPS2LD_LANGS-${OPL_VERSION}.zip" ]
	then echo "OPL Lang Package Complete: OPNPS2LD_LANGS-${OPL_VERSION}.zip"
	else echo "OPL Lang Package not found!"
fi

# Cleanup
cd "${CURRENT_DIR}"
rm -rf ${BUILD_DIR}/ ${LANG_LIST}
unset CURRENT_DIR BUILD_DIR LANG_LIST OPL_VERSION CURRENT_FILE
