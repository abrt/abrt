for f in *.test; do
    b="${f%%.test}"
    dumpoops -s "$f" >"$b".right 2>&1
done
