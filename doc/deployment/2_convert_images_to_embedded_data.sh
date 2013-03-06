#!/bin/sh
# See http://en.wikipedia.org/wiki/Data_URI_scheme

file="tmp/en-US/html-single/index.html"

test -f "$file" || { echo "Need $file"; exit 1; }

cp "$file" Deployment_Guide.html || exit $?

dir=`dirname "$file"`

# Can use just [^"], but more restricted set (no spaces, ^s etc)
# helps to avoid having paranoid code below.
#
grep -o 'src="[A-Za-z0-9/._-]*"' "$file" \
| sort | uniq \
| while read src; do
	name="${src#src=?}"
	name="${name%?}"
	ext="${name##*.}"
	(cd "$dir" && test -f "$name") || continue
	echo "Replacing $name with data:image/$ext;base64"
	# base64 can be long, can overflow sed command line length limit.
	# we supply the command via -f FILE instead.
	{
		printf "%s" "s^$src^src=\"data:image/$ext;base64,"
		(cd "$dir" && base64 <"$name") | tr -d $'\n'
		echo '"^g'
	} | sed -f - -i Deployment_Guide.html
done

#TODO: embed css
