#!/bin/bash

hgchangeset=`eval hg log | grep changeset: | sed -n 's/changeset: *//p' | sed -n 's/:[^.]*//p'`
hg log | grep summary: | sed -n 's/summary: *//p' >rev_summary
hg log | grep date: | sed -n 's/date: *//p' | sed -n 's/+[0-9]*//p' >rev_date

i=0
changeset_array=$(echo $hgchangeset | tr " " "\n")
for x in $changeset_array; do
	changeset[$i]=$x
	i=$(($i + 1))		
done
echo "found $i revision changesets..."

i=0
while read line; do
	summary[$i]=$line
	i=$(($i + 1))
done <rev_summary
echo "found $i revision summaries..."

i=0
while read line; do
	date[$i]=$line
	i=$(($i + 1))
done <rev_date
echo "found $i revision dates..."

echo "-----------------------------------------------------------------------------" >DETAILED_CHANGELOG
echo "" >>DETAILED_CHANGELOG
echo "  Copyright 2009-2010, Ifcaro & jimmikaelkael" >>DETAILED_CHANGELOG
echo "  Licenced under Academic Free License version 3.0" >>DETAILED_CHANGELOG
echo "  Review Open PS2 Loader README & LICENSE files for further details." >>DETAILED_CHANGELOG
echo "" >>DETAILED_CHANGELOG
echo "-----------------------------------------------------------------------------" >>DETAILED_CHANGELOG
echo "" >>DETAILED_CHANGELOG
echo "Open PS2 Loader detailed ChangeLog:" >>DETAILED_CHANGELOG
echo "" >>DETAILED_CHANGELOG

for ((j=0; j<$i; j++)); do
	echo "rev${changeset[$j]} - ${summary[$j]} - ${date[$j]}" >>DETAILED_CHANGELOG
done

rm rev_summary
rm rev_date
cp -f DETAILED_CHANGELOG ../
rm DETAILED_CHANGELOG

echo "DETAILED_CHANGELOG file created."

