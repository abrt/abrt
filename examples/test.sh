for f in *.test; do
        b="${f%%.test}"
        abrt-dump-oops -o "$f" >"$b".out 2>&1
        if diff -u "$b".right "$b".out >"$b".diff 2>&1; then
                echo "Tested $f: ok"
                rm "$b".out "$b".diff
        else
                echo "=================================================="
                echo "Tested $f: BAD"
                echo "--------------------------------------------------"
                cat "$b.diff"
                echo "=================================================="
        fi
done
