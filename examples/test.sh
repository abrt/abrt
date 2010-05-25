for f in *.test; do
	b="${f%%.test}"
	dumpoops -s "$f" >"$b".out 2>&1
	if diff "$b".out "$b".right >"$b".diff 2>&1; then
		rm "$b".out "$b".diff
	fi
done
