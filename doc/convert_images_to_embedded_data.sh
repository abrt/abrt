#!/bin/sh
# See http://en.wikipedia.org/wiki/Data_URI_scheme

test -f index.html || { echo "Need index.html"; exit 1; }

cp index.html index1.html

# Can use just [^"], but more restricted set (no spaces, ^s etc)
# helps to avoid having paranoid code below.
#
grep -o 'src="[A-Za-z0-9/._-]*"' index.html \
| sort | uniq \
| while read src; do
	name="${src#src=?}"
	name="${name%?}"
	ext="${name##*.}"
	test -f "$name" || continue
	echo "Replacing $name with data:image/$ext;base64"
	# base64 can be long, can overflow sed command line length limit.
	# we supply the command via -f FILE instead.
	{
		printf "%s" "s^$src^src=\"data:image/$ext;base64,"
		base64 <"$name" | tr -d $'\n'
		echo '"^g'
	} | sed -f - -i index1.html
done
