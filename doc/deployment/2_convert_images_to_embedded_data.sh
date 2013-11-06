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
	# Extract FILE.EXT from src="FILE.EXT"
	name="${src#src=?}"
	name="${name%?}"
	# Extract EXT from FILE.EXT
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

exit

# WORKS, BUT DISABLED FOR NOW:
# Embed css
#We handle: <link rel="stylesheet" type="text/css" href="Common_Content/css/default.css"
#TODO: also handle <link rel="stylesheet" media="print" href="..."
#TODO: recursively process .css files - they have file references too,
#otherwise we don't gain much:
#default.css is merely 3 lines:
#  @import url("common.css");
#  @import url("overrides.css");
#  @import url("lang.css");
#and further, those files have file refs like
#  background: #003d6e url(../images/h1-bg.png) top left repeat-x;
#
grep -o '<link rel="stylesheet" type="text/css" href="[A-Za-z0-9/._-]*"' "$file" \
| sort | uniq \
| while read src; do
	name="${src#?link rel=?stylesheet? type=?text?css? href=?}"
	name="${name%?}"
	(cd "$dir" && test -f "$name") || continue
	echo "Replacing $name with data:text/css;base64"
	{
		printf "%s" "s^\"$name\"^\"data:text/css;base64,"
		(cd "$dir" && base64 <"$name") | tr -d $'\n'
		echo '"^g'
	} | sed -f - -i Deployment_Guide.html
done
